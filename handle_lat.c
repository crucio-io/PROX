/*
  Copyright(c) 2010-2016 Intel Corporation.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//#define LAT_DEBUG

#include <rte_cycles.h>
#include <stdio.h>
#include <math.h>

#include "prox_malloc.h"
#include "mbuf_utils.h"
#include "handle_lat.h"
#include "log.h"
#include "task_init.h"
#include "task_base.h"
#include "stats.h"
#include "lconf.h"
#include "quit.h"

static int compare_tx_time(void const *val1, void const *val2)
{
	struct lat_info const *ptr1 = val1;
	struct lat_info const *ptr2 = val2;
	return ptr1->tx_time - ptr2->tx_time;
}

static int compare_queue_id(void const *val1, void const *val2)
{
	struct lat_info const *ptr1 = val1;
	struct lat_info const *ptr2 = val2;
	((1L * (ptr1->port_queue_id - ptr2->port_queue_id)) << 32) | (ptr1->tx_packet_index - ptr2->tx_packet_index);
	return ptr1->tx_time - ptr2->tx_time;
}

static void fix_latency_buffer_tx_time(struct lat_info *lat, uint32_t count)
{
	uint32_t id, time, old_id = 0, old_time = 0, n_overflow = 0;

	for (uint32_t i = 0; i < count; i++) {
		id = lat->port_queue_id;
		time = lat->tx_time;
		if (id == old_id) {
			// Same queue id as previous entry; time should always increase
			if (time < old_time) {
				n_overflow++;
			}
			lat->tx_time += UINT32_MAX * n_overflow;
			old_time = time;
		} else {
			// Different queue_id, time starts again at 0
			old_id = id;
			old_time = 0;
			n_overflow = 0;
		}
	}
}

static struct lat_test *task_lat_get_lat_test(struct task_lat *task)
{
	if (task->use_lt != task->using_lt)
		task->using_lt = task->use_lt;

	return &task->lt[task->using_lt];
}

static void lat_count_remaining_lost_packets(struct task_lat *task)
{
	uint32_t queue_id, n_loss;

	// Need to check if we lost any packet before last packet
	// received Any packet lost AFTER the last packet received
	// cannot be counted.  Such a packet will be counted after
	// both lat and gen restarted
	for (uint32_t j = 0; j < MAX_NB_QUEUES; j++) {
		queue_id = task->tx_packet_index[j] >> PACKET_QUEUE_BITS;
		for (uint32_t i = (task->tx_packet_index[j] + 1) & PACKET_QUEUE_MASK; i < PACKET_QUEUE_SIZE; i++) {
			// We ** might ** have received OOO packets; do not count them as lost next time...
			if (queue_id - task->queue[j][i] != 0) {
				n_loss = (queue_id - task->queue[j][i] - 1) & QUEUE_ID_MASK;
				task->lost_packets += n_loss;
			}
			task->queue[j][i] = - 1;
		}
		for (uint32_t i = 0; i < (task->tx_packet_index[j] & PACKET_QUEUE_MASK); i++) {
			// We ** might ** have received OOO packets; do not count them as lost next time...
			if (task->queue[j][i] - queue_id != 1) {
				n_loss = (queue_id - task->queue[j][i]) & QUEUE_ID_MASK;
				task->lost_packets += n_loss;
			}
			task->queue[j][i] = -1;
		}
		task->queue[j][task->tx_packet_index[j] & PACKET_QUEUE_MASK] = -1;
	}
	struct lat_test *lat_test = task_lat_get_lat_test(task);
	lat_test->lost_packets = task->lost_packets;
	task->lost_packets = 0;
}

static uint32_t lat_latency_buffer_get_min_tsc(struct task_lat *task)
{
	uint32_t min_tsc  = UINT32_MAX;

	for (uint32_t i = 0; i < task->rx_packet_index; i++) {
		if (min_tsc > task->latency_buffer[i].tx_time)
			min_tsc = task->latency_buffer[i].tx_time;
	}

	return min_tsc;
}

static void lat_write_latency_to_file(struct task_lat *task)
{
	uint32_t min_tsc;
	uint32_t n_loss;

	// Do not overflow latency buffer
	if (task->rx_packet_index >= task->latency_buffer_size)
		task->rx_packet_index = task->latency_buffer_size - 1;

	min_tsc = lat_latency_buffer_get_min_tsc(task);

	// Dumping all packet statistics
	fprintf(task->fp_rx, "Latency stats for %ld packets, ordered by rx time\n", task->rx_packet_index);
	fprintf(task->fp_rx, "rx index; queue; tx index; lat (nsec);tx time;\n");
	for (uint32_t i = 0; i < task->rx_packet_index ; i++) {
		fprintf(task->fp_rx, "%d;%d;%d;%ld;%lu;%lu\n",
			task->latency_buffer[i].rx_packet_index,
			task->latency_buffer[i].port_queue_id,
			task->latency_buffer[i].tx_packet_index,
			(((task->latency_buffer[i].lat << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz()),
			((task->latency_buffer[i].rx_time - min_tsc) * 1000000000L) / rte_get_tsc_hz(),
			((task->latency_buffer[i].tx_time - min_tsc)* 1000000000L) / rte_get_tsc_hz());
	}

	// To detect dropped packets, we need to sort them based on TX
	uint32_t prev_tx_packet_index[MAX_NB_QUEUES];
	for (uint32_t i = 0; i < MAX_NB_QUEUES; i++) {
		prev_tx_packet_index[i] = -1;
	}
	plogx_info("Sorting packets based on queue_id\n");
	qsort (task->latency_buffer, task->rx_packet_index, sizeof(struct lat_info), compare_queue_id);
	plogx_info("Adapting tx_time\n");
	fix_latency_buffer_tx_time(task->latency_buffer, task->rx_packet_index);
	plogx_info("Sorting packets based on tx_time\n");
	qsort (task->latency_buffer, task->rx_packet_index, sizeof(struct lat_info), compare_tx_time);
	plogx_info("Sorted packets based on tx_time\n");

	// A packet is marked as dropped if 2 packets received from the same queue are not consecutive
	fprintf(task->fp_tx, "Latency stats for %ld packets, sorted by tx time\n", task->rx_packet_index);
	fprintf(task->fp_tx, "queue;tx index; rx index; lat (nsec);tx time; rx time; tx_err;rx_err\n");

	uint32_t lasts = task->rx_packet_index - 64;
	if (task->rx_packet_index < 64)
		lasts = 0;
	for (uint32_t i = 0; i < lasts; i++) {
		// Log dropped packet
		n_loss = task->latency_buffer[i].tx_packet_index - prev_tx_packet_index[task->latency_buffer[i].port_queue_id] - 1;
		if (n_loss)
			fprintf(task->fp_tx, "===> %d;%d;0;0;0;0; lost %d packets <===\n",
				task->latency_buffer[i].port_queue_id,
				task->latency_buffer[i].tx_packet_index - n_loss, n_loss);
		// Log next packet
#ifdef LAT_DEBUG
		fprintf(task->fp_tx, "%d;%d;%d;%ld;%lu;%lu;%lu;%lu;%d from %d;%lu;%lu;%lu\n",
			task->latency_buffer[i].port_queue_id,
			task->latency_buffer[i].tx_packet_index,
			task->latency_buffer[i].rx_packet_index,
			(((task->latency_buffer[i].lat << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz()),
			(((task->latency_buffer[i].tx_time - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			(((task->latency_buffer[i].rx_time - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			(((task->latency_buffer[i + 64].tx_err) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			(((task->latency_buffer[i].rx_err) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			task->latency_buffer[i].packet_id,
			task->latency_buffer[i].bulk_size,
			((((uint32_t)(task->latency_buffer[i].begin >> LATENCY_ACCURACY) - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			((((uint32_t)(task->latency_buffer[i].before >> LATENCY_ACCURACY) - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			((((uint32_t)(task->latency_buffer[i].after >> LATENCY_ACCURACY) - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz());
#else
		fprintf(task->fp_tx, "%d;%d;%d;%ld;%lu;%lu;%lu;%lu\n",
			task->latency_buffer[i].port_queue_id,
			task->latency_buffer[i].tx_packet_index,
			task->latency_buffer[i].rx_packet_index,
			(((task->latency_buffer[i].lat << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz()),
			(((task->latency_buffer[i].tx_time - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			(((task->latency_buffer[i].rx_time - min_tsc) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			(((task->latency_buffer[i + 64].tx_err) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz(),
			(((task->latency_buffer[i].rx_err) << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz());
#endif
		prev_tx_packet_index[task->latency_buffer[i].port_queue_id] = task->latency_buffer[i].tx_packet_index;
	}
	for (uint32_t i =  lasts; i < task->rx_packet_index; i++) {

		// Log dropped packet
		n_loss = task->latency_buffer[i].tx_packet_index - prev_tx_packet_index[task->latency_buffer[i].port_queue_id] - 1;
		if (n_loss)
			fprintf(task->fp_tx, "===> %d;%d;0;0;0;0; lost %d packets <===\n",
				task->latency_buffer[i].port_queue_id,
				task->latency_buffer[i].tx_packet_index - n_loss, n_loss);
		// Log next packet
		fprintf(task->fp_tx, "%d;%d;%d;%ld;%lu;%lu;%u;%u\n",
			task->latency_buffer[i].port_queue_id,
			task->latency_buffer[i].tx_packet_index,
			task->latency_buffer[i].rx_packet_index,
			(((task->latency_buffer[i].lat << LATENCY_ACCURACY) * 1000000000L) / rte_get_tsc_hz()),
			task->latency_buffer[i].tx_time - min_tsc,
			task->latency_buffer[i].rx_time - min_tsc,
			0,
			task->latency_buffer[i].rx_err);
		prev_tx_packet_index[task->latency_buffer[i].port_queue_id] = task->latency_buffer[i].tx_packet_index;
	}
	fflush(task->fp_rx);
	fflush(task->fp_tx);
	task->rx_packet_index = 0;
}

static void lat_stop(struct task_base *tbase)
{
	struct task_lat *task = (struct task_lat *)tbase;

	if (task->packet_id_pos)
		lat_count_remaining_lost_packets(task);
	if (task->latency_buffer)
		lat_write_latency_to_file(task);
}

static void task_lat_store_lat_buf(struct task_lat *task, uint32_t rx_packet_index, uint32_t tx_packet_index, uint8_t port_queue_id, uint64_t lat, uint64_t rx_tsc, uint64_t tx_tsc, uint64_t rx_err, uint64_t tx_err)
{
	struct lat_info *lat_info;

	/* If packet_id_pos is specified then latency is stored per
	   packet being sent. Lost packets are detected runtime, and
	   latency stored for those packets will be 0 */
	lat_info = &task->latency_buffer[rx_packet_index];
	lat_info->rx_packet_index = rx_packet_index;
	lat_info->tx_packet_index = tx_packet_index;
	lat_info->port_queue_id = port_queue_id;
	lat_info->lat = lat;
	lat_info->rx_time = rx_tsc;
	lat_info->tx_time = tx_tsc;
	lat_info->tx_err = tx_err;
	lat_info->rx_err = rx_err;
}

static uint32_t task_lat_early_loss_detect(struct task_lat *task, uint8_t port_queue_id, uint32_t packet_index)
{
	uint32_t old_queue_id, queue_pos, n_loss;

	queue_pos = packet_index & PACKET_QUEUE_MASK;
	old_queue_id = task->queue[port_queue_id][queue_pos];
	task->tx_packet_index[port_queue_id] = packet_index;
	task->queue[port_queue_id][queue_pos] = packet_index >> PACKET_QUEUE_BITS;
	n_loss = (task->queue[port_queue_id][queue_pos] - old_queue_id - 1) & QUEUE_ID_MASK;
	task->lost_packets += n_loss;
	return n_loss;
}

static uint32_t abs_diff(uint32_t a, uint32_t b)
{
       return a < b? UINT32_MAX - (b - a - 1) : a - b;
}

static uint64_t tsc_extrapolate_backward(uint64_t tsc_from, uint64_t bytes, uint64_t tsc_minimum, uint8_t through_ring)
{
	if (through_ring)
		return tsc_from;
	uint64_t tsc = tsc_from - rte_get_tsc_hz()*bytes/1250000000;
	if (likely(tsc > tsc_minimum))
		return tsc;
	else
		return tsc_minimum;
}

static void lat_test_histogram_add(struct task_lat *task, struct lat_test *lat_test, uint64_t lat_time)
{
	uint64_t bucket_id = (lat_time >> task->bucket_size);
	size_t bucket_count = sizeof(lat_test->buckets)/sizeof(lat_test->buckets[0]);

	bucket_id = bucket_id < bucket_count? bucket_id : bucket_count;
	lat_test->buckets[bucket_id]++;
}

static void lat_test_update(struct task_lat *task, struct lat_test *lat_test, uint64_t lat_time, uint64_t rx_err, uint64_t tx_err, uint32_t lost_pkts)
{
	/* tx_err gives the error on TX (i.e. generation side) for the
	   packet that has been sent 64 packets ago. It is difficult
	   to associate it with a received packet, as the packet might
	   even not have been received yet. So the min_tx_acc and
	   max_tx_acc will give the absolute min and max of tx_err */
	lat_test->lost_packets = lost_pkts;
	lat_test->tot_lat += lat_time;
	lat_test->tot_rx_acc += rx_err;
	lat_test->tot_tx_acc += tx_err;
	lat_test->tot_pkts++;

	if (lat_time > lat_test->max_lat) {
		lat_test->max_lat = lat_time;
		lat_test->max_rx_acc = rx_err;
	}
	if (lat_time < lat_test->min_lat) {
		lat_test->min_lat = lat_time;
		lat_test->min_rx_acc = rx_err;
	}
	if (tx_err < lat_test->min_tx_acc) {
		lat_test->min_tx_acc = tx_err;
	}
	if (tx_err > lat_test->max_tx_acc) {
		lat_test->max_tx_acc = tx_err;
	}

#ifndef NO_LATENCY_PER_PACKET
	lat_test->lat[lat_test->cur_pkt++] = lat_time;
	if (lat_test->cur_pkt == MAX_PACKETS_FOR_LATENCY)
		lat_test->cur_pkt = 0;
#endif

#ifndef NO_LATENCY_DETAILS
	lat_test->var_lat += lat_time*lat_time;
	lat_test_histogram_add(task, lat_test, lat_time);
#endif
}

static int task_lat_can_store_latency(struct task_lat *task, uint32_t rx_packet_index)
{
	return rx_packet_index < task->latency_buffer_size;
}

static void handle_lat_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_lat *task = (struct task_lat *)tbase;
	struct lat_test *lat_test;
	uint64_t rx_tsc_err;

	uint32_t pkt_rx_time, pkt_rx_time_verified, pkt_tx_time;
	uint64_t bytes_since_last_pkt = 0;
	uint64_t lat_time = 0;
	uint8_t port_queue_id = 0;
	struct rte_mbuf **m;

	lat_test = task_lat_get_lat_test(task);

	uint16_t tot = 0;

	// If 64 packets or more, buffer them.
	if (n_pkts == 64) {
		memcpy(task->mbufs + tot, mbufs, n_pkts * sizeof(mbufs));
		tot += n_pkts;
		while ((n_pkts == 64) && (tot <= task->mbuf_size - 64)) {
			n_pkts = tbase->rx_pkt(tbase, &mbufs);
			memcpy(task->mbufs + tot, mbufs, n_pkts * sizeof(mbufs));
			tot += n_pkts;
		}
		if (n_pkts == 64) {
			plog_err("handle_lat unable to catch up - buffer full\n");
		}
		m = task->mbufs;
		n_pkts = tot;
	} else {
		m = mbufs;
	}
	const uint64_t rx_tsc = tbase->aux->tsc_rx.after;

	/* Only record latency for first packets */
	if (task->rx_packet_index >= UINT32_MAX - n_pkts)
		task->rx_packet_index = UINT32_MAX - n_pkts;
	else
		task->rx_packet_index += n_pkts;

	uint32_t rx_packet_index = task->rx_packet_index - 1;
	uint32_t tx_packet_index = 0;
	uint32_t tx_tsc_err = 0;
	uint8_t *hdr;

	// Go once through all received packets and read them
	// If packet has just been modified by another core,
	// the cost of latency will be partialy amortized though the bulk size
	for (uint16_t j = 0; j < n_pkts; ++j) {
		struct rte_mbuf *mbuf = m[n_pkts - 1 - j];
		task->hdr[j] = rte_pktmbuf_mtod(mbuf, uint8_t *);
	}
	for (uint16_t j = 0; j < n_pkts; ++j) {
		task->pkt_tx_time[j] = *(uint32_t *)(task->hdr[j] + task->lat_pos);
	}

	// Find RX time of first packet, for RX accuracy
	for (uint16_t j = 1; j < n_pkts; ++j) {
		hdr = task->hdr[j];
		bytes_since_last_pkt += mbuf_wire_size(m[n_pkts - 1 - j]);
	}
	pkt_rx_time = tsc_extrapolate_backward(rx_tsc, bytes_since_last_pkt, task->last_pkts_tsc, task->through_ring) >> LATENCY_ACCURACY;
	if ((uint32_t)((tbase->aux->tsc_rx.begin >> LATENCY_ACCURACY)) > pkt_rx_time) {
		// Extrapolation went up to BEFORE begin => packets were stuck in the NIC but we were not seeing them
		rx_tsc_err = pkt_rx_time - (uint32_t)((task->last_pkts_tsc >> LATENCY_ACCURACY));
	} else {
		rx_tsc_err = pkt_rx_time - (uint32_t)((tbase->aux->tsc_rx.begin >> LATENCY_ACCURACY));
	}

	bytes_since_last_pkt = 0;
	for (uint16_t j = 0; j < n_pkts; ++j) {
		hdr = task->hdr[j];
		pkt_rx_time = tsc_extrapolate_backward(rx_tsc, bytes_since_last_pkt, task->last_pkts_tsc, task->through_ring) >> LATENCY_ACCURACY;
		if (task->accur_pos) {
			tx_tsc_err = *(uint32_t *)(hdr + task->accur_pos);
		}
		pkt_tx_time = task->pkt_tx_time[j];
		lat_time = abs_diff(pkt_rx_time, pkt_tx_time);

		if (task->packet_id_pos) {
			tx_packet_index = *(uint32_t *)(hdr + task->packet_id_pos + 1);
			port_queue_id = *(uint8_t *)(hdr + task->packet_id_pos);
			PROX_PANIC(port_queue_id >= MAX_NB_QUEUES, "Received packet with unexpected port_queue_id written in packet\n");
			task_lat_early_loss_detect(task, port_queue_id, tx_packet_index);
		}


		if (task_lat_can_store_latency(task, rx_packet_index)) {
			task_lat_store_lat_buf(task, rx_packet_index, tx_packet_index, port_queue_id, lat_time, pkt_rx_time, pkt_tx_time, rx_tsc_err, tx_tsc_err);
		}

		lat_test_update(task, lat_test, lat_time, rx_tsc_err, tx_tsc_err, task->lost_packets);
		bytes_since_last_pkt += mbuf_wire_size(m[n_pkts - 1 - j]);
		rx_packet_index--;
	}
#ifdef LAT_DEBUG
	if (task_lat_can_store_latency(task, task->rx_packet_index)) {
		for (uint16_t j = 0; j < n_pkts; ++j) {
			task->latency_buffer[task->rx_packet_index + j - n_pkts].bulk_size = n_pkts;
			task->latency_buffer[task->rx_packet_index + j - n_pkts].packet_id = j;
			task->latency_buffer[task->rx_packet_index + j - n_pkts].begin = tbase->aux->tsc_rx.begin;
			task->latency_buffer[task->rx_packet_index + j - n_pkts].before = tbase->aux->tsc_rx.before;
			task->latency_buffer[task->rx_packet_index + j - n_pkts].after = tbase->aux->tsc_rx.after;
		}
	}
#endif
	task->base.tx_pkt(&task->base, m, n_pkts, NULL);
	tbase->aux->tsc_rx.begin = tbase->aux->tsc_rx.before;
	task->last_pkts_tsc = tbase->aux->tsc_rx.after;
}

static void init_task_lat_latency_buffer(struct task_lat *task, uint32_t core_id)
{
	const int socket_id = rte_lcore_to_socket_id(core_id);
	char name[256];
	size_t latency_buffer_mem_size = 0;

	if (task->latency_buffer_size > UINT32_MAX - MAX_RING_BURST)
		task->latency_buffer_size = UINT32_MAX - MAX_RING_BURST;

	latency_buffer_mem_size = sizeof(struct lat_info) * task->latency_buffer_size;

	task->latency_buffer = prox_zmalloc(latency_buffer_mem_size, socket_id);
	PROX_PANIC(task->latency_buffer == NULL, "Failed to allocate %ld kbytes for %s\n", latency_buffer_mem_size / 1024, name);

	sprintf(name, "latency.rx_%d.txt", core_id);
	task->fp_rx = fopen(name, "w+");
	PROX_PANIC(task->fp_rx == NULL, "Failed to open %s\n", name);

	sprintf(name, "latency.tx_%d.txt", core_id);
	task->fp_tx = fopen(name, "w+");
	PROX_PANIC(task->fp_tx == NULL, "Failed to open %s\n", name);
}

static void init_task_lat(struct task_base *tbase, struct task_args *targ)
{
	struct task_lat *task = (struct task_lat *)tbase;
	const int socket_id = rte_lcore_to_socket_id(targ->lconf->id);

	task->lat_pos = targ->lat_pos;
	task->accur_pos = targ->accur_pos;
	task->packet_id_pos = targ->packet_id_pos;
	task->latency_buffer_size = targ->latency_buffer_size;

	if (task->latency_buffer_size) {
		init_task_lat_latency_buffer(task, targ->lconf->id);
	}

	if (targ->bucket_size < LATENCY_ACCURACY) {
		// Latency data is already shifted by LATENCY_ACCURACY
		task->bucket_size = DEFAULT_BUCKET_SIZE - LATENCY_ACCURACY; // each bucket will hold 1024 cycles by default
	} else {
		task->bucket_size = targ->bucket_size - LATENCY_ACCURACY;
	}
        if (task->packet_id_pos) {
		for (int j = 0; j< MAX_NB_QUEUES; j++) {
                	for (int i =0; i< PACKET_QUEUE_SIZE; i++) {
                        	task->queue[j][i] = -1;
                	}
               	}
        }
	task->mbuf_size = 16384; // more or less 1 msec
	task->mbufs = prox_zmalloc(task->mbuf_size * sizeof(* task->mbufs), socket_id);
	PROX_PANIC(task->mbufs == NULL, "unable to allocate memory to store received mbufs pointers");
	task->pkt_tx_time = prox_zmalloc(task->mbuf_size * sizeof(uint32_t), socket_id);
	PROX_PANIC(task->mbufs == NULL, "unable to allocate memory to store tx accuracy");
	task->hdr = prox_zmalloc(task->mbuf_size * sizeof(uint8_t *), socket_id);
	PROX_PANIC(task->mbufs == NULL, "unable to allocate memory to store hdr");
	if ((targ->nb_txrings == 1) && (targ->nb_txports == 0)) {
		task->through_ring = 1;
	}
}

static struct task_init task_init_lat = {
	.mode_str = "lat",
	.init = init_task_lat,
	.handle = handle_lat_bulk,
	.stop = lat_stop,
	.flag_features = TASK_FEATURE_TSC_RX | TASK_FEATURE_TWICE_RX | TASK_FEATURE_NEVER_DISCARDS,
	.size = sizeof(struct task_lat)
};

__attribute__((constructor)) static void reg_task_lat(void)
{
	reg_task(&task_init_lat);
}
