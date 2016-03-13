/*
  Copyright(c) 2010-2015 Intel Corporation.
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

#include <rte_ethdev.h>
#include <rte_version.h>

#include "tx_pkt.h"
#include "task_base.h"
#include "stats.h"
#include "prefetch.h"
#include "prox_assert.h"
#include "log.h"
#include "mbuf_utils.h"

static void tx_buf_pkt_single(struct task_base *tbase, struct rte_mbuf *mbuf, const uint8_t out)
{
	const uint16_t prod = tbase->ws_mbuf->idx[out].prod++;
	tbase->ws_mbuf->mbuf[out][prod & WS_MBUF_MASK] = mbuf;
}

/* The following help functions also report stats. Therefore we need
   to pass the task_base struct. */
static inline void tx_drop(const struct port_queue *port_queue, struct rte_mbuf **mbufs, uint16_t n_pkts, __attribute__((unused)) struct task_base *tbase)
{
	uint16_t ntx = rte_eth_tx_burst(port_queue->port, port_queue->queue, mbufs, n_pkts);

	TASK_STATS_ADD_TX(&tbase->aux->stats, ntx);
	if (ntx < n_pkts) {
		TASK_STATS_ADD_DROP_TX_FAIL(&tbase->aux->stats, n_pkts - ntx);
		if (tbase->tx_pkt == tx_pkt_bw) {
			uint32_t drop_bytes = 0;
			do {
				drop_bytes += mbuf_wire_size(mbufs[ntx]);
				rte_pktmbuf_free(mbufs[ntx++]);
			} while (ntx < n_pkts);
			TASK_STATS_ADD_DROP_BYTES(&tbase->aux->stats, drop_bytes);
		}
		else {
			do {
				rte_pktmbuf_free(mbufs[ntx++]);
			} while (ntx < n_pkts);
		}
	}
}

static inline void tx_no_drop(const struct port_queue *port_queue, struct rte_mbuf **mbufs, uint16_t n_pkts, __attribute__((unused)) struct task_base *tbase)
{
	uint16_t ret;

	TASK_STATS_ADD_TX(&tbase->aux->stats, n_pkts);

	do {
		ret = rte_eth_tx_burst(port_queue->port, port_queue->queue, mbufs, n_pkts);
		mbufs += ret;
		n_pkts -= ret;
	}
	while (n_pkts);
}

static inline void ring_enq_drop(struct rte_ring *ring, struct rte_mbuf *const *mbufs, uint16_t n_pkts, __attribute__((unused)) struct task_base *tbase)
{
	/* return 0 on succes, -ENOBUFS on failure */
	// Rings can be single or multiproducer (ctrl rings are multi producer)
	if (unlikely(rte_ring_enqueue_bulk(ring, (void *const *)mbufs, n_pkts))) {
		if (tbase->tx_pkt == tx_pkt_bw) {
			uint32_t drop_bytes = 0;
			for (uint16_t i = 0; i < n_pkts; ++i) {
				drop_bytes += mbuf_wire_size(mbufs[i]);
				rte_pktmbuf_free(mbufs[i]);
			}
			TASK_STATS_ADD_DROP_BYTES(&tbase->aux->stats, drop_bytes);
			TASK_STATS_ADD_DROP_TX_FAIL(&tbase->aux->stats, n_pkts);
		}
		else {
			for (uint16_t i = 0; i < n_pkts; ++i)
				rte_pktmbuf_free(mbufs[i]);
			TASK_STATS_ADD_DROP_TX_FAIL(&tbase->aux->stats, n_pkts);
		}
	}
	else {
		TASK_STATS_ADD_TX(&tbase->aux->stats, n_pkts);
	}
}

static inline void ring_enq_no_drop(struct rte_ring *ring, struct rte_mbuf *const *mbufs, uint16_t n_pkts, __attribute__((unused)) struct task_base *tbase)
{
	while (rte_ring_enqueue_bulk(ring, (void *const *)mbufs, n_pkts));
	TASK_STATS_ADD_TX(&tbase->aux->stats, n_pkts);
}

void flush_queues_hw(struct task_base *tbase)
{
	uint16_t prod, cons;

	for (uint8_t i = 0; i < tbase->tx_params_hw.nb_txports; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (prod != cons) {
			tbase->ws_mbuf->idx[i].prod = 0;
			tbase->ws_mbuf->idx[i].cons = 0;
			tx_drop(&tbase->tx_params_hw.tx_port_queue[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), prod - cons, tbase);
		}
	}

	tbase->flags &= ~FLAG_TX_FLUSH;
}

void flush_queues_sw(struct task_base *tbase)
{
	uint16_t prod, cons;

	for (uint8_t i = 0; i < tbase->tx_params_sw.nb_txrings; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (prod != cons) {
			tbase->ws_mbuf->idx[i].prod = 0;
			tbase->ws_mbuf->idx[i].cons = 0;
			ring_enq_drop(tbase->tx_params_sw.tx_rings[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), prod - cons, tbase);
		}
	}
	tbase->flags &= ~FLAG_TX_FLUSH;
}

void flush_queues_no_drop_hw(struct task_base *tbase)
{
	uint16_t prod, cons;

	for (uint8_t i = 0; i < tbase->tx_params_hw.nb_txports; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (prod != cons) {
			tbase->ws_mbuf->idx[i].prod = 0;
			tbase->ws_mbuf->idx[i].cons = 0;
			tx_no_drop(&tbase->tx_params_hw.tx_port_queue[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), prod - cons, tbase);
		}
	}

	tbase->flags &= ~FLAG_TX_FLUSH;
}

void flush_queues_no_drop_sw(struct task_base *tbase)
{
	uint16_t prod, cons;

	for (uint8_t i = 0; i < tbase->tx_params_sw.nb_txrings; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (prod != cons) {
			tbase->ws_mbuf->idx[i].prod = 0;
			tbase->ws_mbuf->idx[i].cons = 0;
			ring_enq_no_drop(tbase->tx_params_sw.tx_rings[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), prod - cons, tbase);
		}
	}
	tbase->flags &= ~FLAG_TX_FLUSH;
}

void tx_pkt_no_drop_never_discard_hw1_lat_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	tx_no_drop(&tbase->tx_params_hw.tx_port_queue[0], mbufs, n_pkts, tbase);
}

void tx_pkt_no_drop_never_discard_hw1_thrpt_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	static uint8_t fake_out[MAX_PKT_BURST] = {0};
	if (n_pkts == MAX_PKT_BURST) {
		// First xmit what was queued
        	uint16_t prod, cons;

               	prod = tbase->ws_mbuf->idx[0].prod;
               	cons = tbase->ws_mbuf->idx[0].cons;

		if ((uint16_t)(prod - cons)){
                	tbase->flags &= ~FLAG_TX_FLUSH;
                	tbase->ws_mbuf->idx[0].prod = 0;
                	tbase->ws_mbuf->idx[0].cons = 0;
                	tx_no_drop(&tbase->tx_params_hw.tx_port_queue[0], tbase->ws_mbuf->mbuf[0] + (cons & WS_MBUF_MASK), (uint16_t)(prod - cons), tbase);
		}
		tx_no_drop(&tbase->tx_params_hw.tx_port_queue[0], mbufs, n_pkts, tbase);
	} else {
		tx_pkt_no_drop_hw(tbase, mbufs, n_pkts, fake_out);
	}
}

void tx_pkt_no_drop_never_discard_hw1_no_pointer(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	tx_no_drop(&tbase->tx_params_hw_sw.tx_port_queue, mbufs, n_pkts, tbase);
}

void tx_pkt_no_drop_never_discard_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	ring_enq_no_drop(tbase->tx_params_sw.tx_rings[0], mbufs, n_pkts, tbase);
}

void tx_pkt_never_discard_hw1_lat_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	tx_drop(&tbase->tx_params_hw.tx_port_queue[0], mbufs, n_pkts, tbase);
}

void tx_pkt_never_discard_hw1_thrpt_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	static uint8_t fake_out[MAX_PKT_BURST] = {0};
	if (n_pkts == MAX_PKT_BURST) {
		// First xmit what was queued
        	uint16_t prod, cons;

               	prod = tbase->ws_mbuf->idx[0].prod;
               	cons = tbase->ws_mbuf->idx[0].cons;

		if ((uint16_t)(prod - cons)){
                	tbase->flags &= ~FLAG_TX_FLUSH;
                	tbase->ws_mbuf->idx[0].prod = 0;
                	tbase->ws_mbuf->idx[0].cons = 0;
                	tx_drop(&tbase->tx_params_hw.tx_port_queue[0], tbase->ws_mbuf->mbuf[0] + (cons & WS_MBUF_MASK), (uint16_t)(prod - cons), tbase);
		}
		tx_drop(&tbase->tx_params_hw.tx_port_queue[0], mbufs, n_pkts, tbase);
	} else {
		tx_pkt_hw(tbase, mbufs, n_pkts, fake_out);
	}
}

void tx_pkt_never_discard_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	ring_enq_drop(tbase->tx_params_sw.tx_rings[0], mbufs, n_pkts, tbase);
}

static uint16_t tx_pkt_free_dropped(__attribute__((unused)) struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out)
{
	uint64_t v = 0;
	uint16_t i;
	/* The most probable and most important optimize case is if
	   the no packets should be dropped. */
	for (i = 0; i + 8 < n_pkts; i += 8) {
		v |= *((uint64_t*)(&out[i]));
	}
	for (; i < n_pkts; ++i) {
		v |= out[i];
	}


	if (unlikely(v)) {
		/* At least some packets need to be dropped, so the
		   mbufs array needs to be updated. */
		uint16_t n_kept = 0;
		uint16_t n_discard = 0;
		for (uint16_t i = 0; i < n_pkts; ++i) {
			if (unlikely(out[i] >= OUT_HANDLED)) {
				rte_pktmbuf_free(mbufs[i]);
				n_discard += out[i] == OUT_DISCARD;
				continue;
			}
			mbufs[n_kept++] = mbufs[i];
		}
		TASK_STATS_ADD_DROP_DISCARD(&tbase->aux->stats, n_discard);
		TASK_STATS_ADD_DROP_HANDLED(&tbase->aux->stats, n_pkts - n_kept - n_discard);
		return n_kept;
	}
	return n_pkts;
}

void tx_pkt_no_drop_hw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out)
{
	const uint16_t n_kept = tx_pkt_free_dropped(tbase, mbufs, n_pkts, out);

	if (likely(n_kept))
		tx_no_drop(&tbase->tx_params_hw.tx_port_queue[0], mbufs, n_kept, tbase);
}

void tx_pkt_no_drop_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out)
{
	const uint16_t n_kept = tx_pkt_free_dropped(tbase, mbufs, n_pkts, out);

	if (likely(n_kept))
		ring_enq_no_drop(tbase->tx_params_sw.tx_rings[0], mbufs, n_kept, tbase);
}

void tx_pkt_hw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out)
{
	const uint16_t n_kept = tx_pkt_free_dropped(tbase, mbufs, n_pkts, out);

	if (likely(n_kept))
		tx_drop(&tbase->tx_params_hw.tx_port_queue[0], mbufs, n_kept, tbase);
}

void tx_pkt_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out)
{
	const uint16_t n_kept = tx_pkt_free_dropped(tbase, mbufs, n_pkts, out);

	if (likely(n_kept))
		ring_enq_drop(tbase->tx_params_sw.tx_rings[0], mbufs, n_kept, tbase);
}

void tx_pkt_self(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out)
{
	const uint16_t n_kept = tx_pkt_free_dropped(tbase, mbufs, n_pkts, out);

	TASK_STATS_ADD_TX(&tbase->aux->stats, n_kept);
	tbase->ws_mbuf->idx[0].nb_rx = n_kept;
	struct rte_mbuf **tx_mbuf = tbase->ws_mbuf->mbuf[0] + (tbase->ws_mbuf->idx[0].prod & WS_MBUF_MASK);
	for (uint16_t i = 0; i < n_kept; ++i) {
		tx_mbuf[i] = mbufs[i];
	}
}

void tx_pkt_never_discard_self(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, __attribute__((unused)) uint8_t *out)
{
	TASK_STATS_ADD_TX(&tbase->aux->stats, n_pkts);
	tbase->ws_mbuf->idx[0].nb_rx = n_pkts;
	struct rte_mbuf **tx_mbuf = tbase->ws_mbuf->mbuf[0] + (tbase->ws_mbuf->idx[0].prod & WS_MBUF_MASK);
	for (uint16_t i = 0; i < n_pkts; ++i) {
		tx_mbuf[i] = mbufs[i];
	}
}

static inline void tx_pkt_buf_all(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	for (uint16_t j = 0; j < n_pkts; ++j) {
		if (unlikely(out[j] >= OUT_HANDLED)) {
			rte_pktmbuf_free(mbufs[j]);
			if (out[j] == OUT_HANDLED)
				TASK_STATS_ADD_DROP_HANDLED(&tbase->aux->stats, 1);
			else
				TASK_STATS_ADD_DROP_DISCARD(&tbase->aux->stats, 1);
		}
		else {
			tx_buf_pkt_single(tbase, mbufs[j], out[j]);
		}
	}
}

void tx_pkt_no_drop_hw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	tx_pkt_buf_all(tbase, mbufs, n_pkts, out);

	const uint8_t nb_bufs = tbase->tx_params_hw.nb_txports;
	uint16_t prod, cons;

	for (uint8_t i = 0; i < nb_bufs; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (((uint16_t)(prod - cons)) >= MAX_PKT_BURST) {
			tbase->flags &= ~FLAG_TX_FLUSH;
			tbase->ws_mbuf->idx[i].cons = cons + MAX_PKT_BURST;
			tx_no_drop(&tbase->tx_params_hw.tx_port_queue[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), MAX_PKT_BURST, tbase);
		}
	}
}

void tx_pkt_no_drop_sw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	tx_pkt_buf_all(tbase, mbufs, n_pkts, out);

	const uint8_t nb_bufs = tbase->tx_params_sw.nb_txrings;
	uint16_t prod, cons;

	for (uint8_t i = 0; i < nb_bufs; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (((uint16_t)(prod - cons)) >= MAX_PKT_BURST) {
			tbase->flags &= ~FLAG_TX_FLUSH;
			tbase->ws_mbuf->idx[i].cons = cons + MAX_PKT_BURST;
			ring_enq_no_drop(tbase->tx_params_sw.tx_rings[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), MAX_PKT_BURST, tbase);
		}
	}
}

void tx_pkt_hw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	tx_pkt_buf_all(tbase, mbufs, n_pkts, out);

	const uint8_t nb_bufs = tbase->tx_params_hw.nb_txports;
	uint16_t prod, cons;

	for (uint8_t i = 0; i < nb_bufs; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (((uint16_t)(prod - cons)) >= MAX_PKT_BURST) {
			tbase->flags &= ~FLAG_TX_FLUSH;
			tbase->ws_mbuf->idx[i].cons = cons + MAX_PKT_BURST;
			tx_drop(&tbase->tx_params_hw.tx_port_queue[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), MAX_PKT_BURST, tbase);
		}
	}
}

void tx_pkt_sw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	tx_pkt_buf_all(tbase, mbufs, n_pkts, out);

	const uint8_t nb_bufs = tbase->tx_params_sw.nb_txrings;
	uint16_t prod, cons;
	for (uint8_t i = 0; i < nb_bufs; ++i) {
		prod = tbase->ws_mbuf->idx[i].prod;
		cons = tbase->ws_mbuf->idx[i].cons;

		if (((uint16_t)(prod - cons)) >= MAX_PKT_BURST) {
			tbase->flags &= ~FLAG_TX_FLUSH;
			tbase->ws_mbuf->idx[i].cons = cons + MAX_PKT_BURST;
			ring_enq_drop(tbase->tx_params_sw.tx_rings[i], tbase->ws_mbuf->mbuf[i] + (cons & WS_MBUF_MASK), MAX_PKT_BURST, tbase);
		}
	}
}

void tx_pkt_trace(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	plog_info("Tracing %d pkts\n", tbase->aux->task_rt_dump.cur_trace);

	for (uint32_t i = 0; i < tbase->aux->task_rt_dump.cur_trace; ++i) {
		struct rte_mbuf tmp;
		/* For each packet being transmitted, find which
		   buffer represent the packet as it was before
		   processing. */
		uint32_t j = 0;
		uint32_t len = sizeof(tbase->aux->task_rt_dump.pkt_mbuf_addr)/sizeof(tbase->aux->task_rt_dump.pkt_mbuf_addr[0]);
		for (;j < len; ++j) {
			if (tbase->aux->task_rt_dump.pkt_mbuf_addr[j] == mbufs[i])
				break;
		}
		if (j == len) {
			plog_info("Trace RX: missing!\n");
		}
		else {
#if RTE_VERSION >= RTE_VERSION_NUM(1,8,0,0)
			tmp.data_off = 0;
#endif
			rte_pktmbuf_data_len(&tmp) = tbase->aux->task_rt_dump.pkt_cpy_len[j];
			rte_pktmbuf_pkt_len(&tmp) = tbase->aux->task_rt_dump.pkt_cpy_len[j];
			tmp.buf_addr = tbase->aux->task_rt_dump.pkt_cpy;
			plogd_info(&tmp, "Trace RX: ");
		}

		if (out) {
			if (out[i] != 0xFF)
				plogd_info(mbufs[i], "Trace TX[%d]: ", out[i]);
			else
				plogd_info(mbufs[i], "Trace Dropped: ");
		} else
			plogd_info(mbufs[i], "Trace TX: ");
	}
	tbase->aux->tx_pkt_orig(tbase, mbufs, n_pkts, out);

	/* Unset by TX when n_trace = 0 */
	if (0 == tbase->aux->task_rt_dump.n_trace) {
		tbase->tx_pkt = tbase->aux->tx_pkt_orig;
		tbase->aux->tx_pkt_orig = NULL;
		tbase->rx_pkt = tbase->aux->rx_pkt_orig;
		tbase->aux->rx_pkt_orig = NULL;
	}
}

void tx_pkt_dump(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	uint32_t n_dump = tbase->aux->task_rt_dump.n_print_tx;

	n_dump = n_pkts < n_dump? n_pkts : n_dump;
	for (uint32_t i = 0; i < n_dump; ++i) {
		if (out)
			plogd_info(mbufs[i], "TX[%d]: ", out[i]);
		else
			plogd_info(mbufs[i], "TX: ");
	}
	tbase->aux->task_rt_dump.n_print_tx -= n_dump;

	tbase->aux->tx_pkt_orig(tbase, mbufs, n_pkts, out);

	if (0 == tbase->aux->task_rt_dump.n_print_tx) {
		tbase->tx_pkt = tbase->aux->tx_pkt_orig;
		tbase->aux->tx_pkt_orig = NULL;
	}
}

/* Gather the distribution of the number of packets that have been
   xmitted from one TX call. Since the value is only modified by the
   task that xmits the packet, no atomic operation is needed. */
void tx_pkt_distr(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	tbase->aux->tx_bucket[n_pkts]++;
	tbase->aux->tx_pkt_orig(tbase, mbufs, n_pkts, out);
}

void tx_pkt_bw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	uint32_t tx_bytes = 0;
	uint32_t drop_bytes = 0;

	for (uint16_t i = 0; i < n_pkts; ++i) {
		if (!out || out[i] < OUT_HANDLED)
			tx_bytes += mbuf_wire_size(mbufs[i]);
		else
			drop_bytes += mbuf_wire_size(mbufs[i]);
	}

	TASK_STATS_ADD_TX_BYTES(&tbase->aux->stats, tx_bytes);
	TASK_STATS_ADD_DROP_BYTES(&tbase->aux->stats, drop_bytes);
	tbase->aux->tx_pkt_orig(tbase, mbufs, n_pkts, out);
}

void tx_pkt_drop_all(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out)
{
	for (uint16_t j = 0; j < n_pkts; ++j) {
		rte_pktmbuf_free(mbufs[j]);
	}
	TASK_STATS_ADD_DROP_HANDLED(&tbase->aux->stats, n_pkts);
}
