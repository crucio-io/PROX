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

#include <rte_mbuf.h>
#include <pcap.h>
#include <string.h>
#include <stdlib.h>
#include <rte_cycles.h>
#include <rte_version.h>
#include <rte_byteorder.h>
#include <rte_ether.h>

#include "prox_malloc.h"
#include "handle_gen.h"
#include "handle_lat.h"
#include "task_init.h"
#include "task_base.h"
#include "prox_port_cfg.h"
#include "lconf.h"
#include "log.h"
#include "quit.h"
#include "prox_cfg.h"
#include "mbuf_utils.h"
#include "qinq.h"
#include "prox_cksum.h"
#include "etypes.h"
#include "prox_assert.h"
#include "prefetch.h"
#include "token_time.h"

struct gen_proto {
	uint16_t len;
	uint8_t  buf[ETHER_MAX_LEN];
};

struct task_gen_pcap {
	struct task_base base;
	struct rte_mempool* mempool;
	uint64_t hz;
	uint32_t pkt_idx;
	struct gen_proto *proto;
	uint32_t loop;
	uint32_t n_pkts;
	uint64_t last_tsc;
	uint64_t *proto_tsc;
	struct rte_mbuf *new_pkts[64];
	uint32_t n_new_pkts;
};

struct task_gen {
	struct task_base base;
	struct rte_mempool* mempool;
	uint64_t hz;
	struct token_time token_time;
	struct gen_proto *proto; /* packet templates (from inline or from pcap) */
	uint64_t write_tsc; /* how long it took previously to write the time stamps in the packets */
	uint64_t time_next_pkt;
	uint64_t new_rate_bps;
	uint64_t pkt_queue_index;
	uint32_t n_pkts; /* number of packets in pcap */
	uint32_t pkt_idx; /* current packet from pcap */
	uint32_t n_new_pkts;
	uint32_t pkt_count; /* how many pakets to generate */
	uint32_t runtime_flags;
	uint16_t lat_pos;
	uint16_t packet_id_pos;
	uint16_t accur_pos;
	uint8_t queue_id;
	uint8_t n_rands; /* number of randoms */
	uint8_t n_values; /* number of fixed values */
	uint8_t min_bulk_size;
	uint8_t max_bulk_size;
	uint8_t through_ring;
	uint8_t lat_enabled;
	struct {
		uint32_t rand_mask; /* since the random vals are uniform, masks don't introduce bias  */
		uint32_t fixed_bits; /* length of each random (max len = 4) */
		uint32_t seeds;
		uint16_t rand_offset; /* each random has an offset*/
		uint8_t rand_len; /* # bytes to take from random (no bias introduced) */
	} rand[64];
	struct {
		uint32_t value; /* value of fixed byte */
		uint16_t offset; /* offset of bytes with fixed value */
		uint8_t value_len; /* length of fixed byte */
	} fixed[64];
	struct rte_mbuf *new_pkts[64];
	uint64_t accur[64];
	struct {
		uint64_t tsc_offset;
		uint32_t *pos;
	} pkt_tsc[64];
} __rte_cache_aligned;

#ifndef RTE_CACHE_LINE_SIZE
#define RTE_CACHE_LINE_SIZE CACHE_LINE_SIZE
#endif

static inline uint8_t init_l3_len(struct ipv4_hdr *ip, uint8_t *pkt, uint8_t l2_len)
{
	uint8_t l3_len = sizeof(struct ipv4_hdr);
	if (unlikely(ip->version_ihl >> 4 != 4)) {
		plog_warn("IPv4 ether_type but IP version = %d != 4", ip->version_ihl >> 4);
		return 0;
	}
	if (unlikely((ip->version_ihl & 0xF) != 5)) {
		l3_len = (ip->version_ihl & 0xF) * 4;
	}
	return l3_len;
}

static void checksum_packet(struct ether_hdr *hdr, struct rte_mbuf *mbuf)
{
	uint8_t l2_len = sizeof(struct ether_hdr), l3_len = 0;
	struct ipv4_hdr *ip = NULL;
	uint8_t *pkt = (uint8_t *)hdr;
	struct vlan_hdr *vlan_hdr;
	struct qinq_hdr *qinq_hdr;

	switch (hdr->ether_type) {
	case ETYPE_IPv6:
		// No L3 cksum offload, but TODO L4 offload
		l2_len = 0;
		break;
	case ETYPE_MPLSU:
	case ETYPE_MPLSM:
		l2_len +=4;
	case ETYPE_IPv4:
		// Initialize l3_len and l3 header csum for IP CSUM offload.
		ip = (struct ipv4_hdr *)(pkt + l2_len);
		l3_len = init_l3_len(ip, pkt, l2_len);
		break;
	case ETYPE_EoGRE:
		l2_len = 0;
		// Not implemented yet
		break;
	case ETYPE_8021ad:
	case ETYPE_VLAN:
		// vlan_hdr is only VLAN header i.e. 4 bytes, starts after Ethernet header
		// qinq_hdr is Ethernet jeader + qinq i.e. 22 bytes, starts at Ethernet header
		vlan_hdr = (struct vlan_hdr *)(pkt + l2_len);
		qinq_hdr = (struct qinq_hdr *)hdr;
		l2_len +=4;
		switch (qinq_hdr->cvlan.eth_proto) {
		case ETYPE_IPv6:
			l2_len = 0;
			break;
		case ETYPE_VLAN:
			l2_len +=4;
			if (qinq_hdr->ether_type == ETYPE_IPv4) {
				// Update l3_len for IP CSUM offload in case IP header contains optional fields.
				ip = (struct ipv4_hdr *)(qinq_hdr + 1);
				l3_len = init_l3_len(ip, pkt, l2_len);
			} else if (qinq_hdr->ether_type == ETYPE_ARP) {
				l2_len = 0;
			} else {
				l2_len = 0;
			}
			break;
		case ETYPE_IPv4:
			// Update l3_len for IP CSUM offload in case IP header contains optional fields.
			ip = (struct ipv4_hdr *)(vlan_hdr + 1);
			l3_len = init_l3_len(ip, pkt, l2_len);
			break;
		case ETYPE_ARP:
			l2_len = 0;
			break;
		default:
			l2_len = 0;
			plog_warn("Unsupported packet type %x - CRC might be wrong\n", qinq_hdr->cvlan.eth_proto);
			break;
		}
		break;
	case ETYPE_ARP:
		l2_len = 0;
		break;
	default:
		l2_len = 0;
		plog_warn("Unsupported packet type %x - CRC might be wrong\n", hdr->ether_type);
		break;
	}
	if (l2_len) {
		prox_ip_udp_cksum(mbuf, ip, l2_len, l3_len);
	}
}

static void set_random_fields(struct task_gen *task, uint8_t *hdr)
{
	uint32_t ret, ret_tmp;

	for (uint16_t i = 0; i < task->n_rands; ++i) {
		ret = rand_r(&task->rand[i].seeds);
		ret_tmp = (ret & task->rand[i].rand_mask) | task->rand[i].fixed_bits;

		ret_tmp = rte_bswap32(ret_tmp);
		/* At this point, the lower order bytes (BE) contain
		   the generated value. The address where the values
		   of interest starts is at ret_tmp + 4 - rand_len. */
		uint8_t *pret_tmp = (uint8_t*)&ret_tmp;
		rte_memcpy(hdr + task->rand[i].rand_offset, pret_tmp + 4 - task->rand[i].rand_len, task->rand[i].rand_len);
	}
}

static void set_fixed_fields(struct task_gen *task, uint8_t *hdr)
{
	for (uint16_t i = 0; i < task->n_values; ++i)
		rte_memcpy(hdr + task->fixed[i].offset, &task->fixed[i].value, task->fixed[i].value_len);
}

static void task_gen_reset_token_time(struct task_gen *task)
{
	token_time_set_bpp(&task->token_time, task->new_rate_bps);
	token_time_reset(&task->token_time, rte_rdtsc(), 0);
}

static void start(struct task_base *tbase)
{
	struct task_gen *task = (struct task_gen *)tbase;
	task->pkt_queue_index = 0;

	task_gen_reset_token_time(task);
}

static void start_pcap(struct task_base *tbase)
{
	struct task_gen_pcap *task = (struct task_gen_pcap *)tbase;
	/* When we start, the first packet is sent immediatly. */
	task->last_tsc = rte_rdtsc() - task->proto_tsc[0];
	task->pkt_idx = 0;
}

static void task_gen_refill_new_pkts(struct task_gen *task)
{
	uint32_t fill = 64 - task->n_new_pkts;
	if (rte_mempool_get_bulk(task->mempool, (void **)task->new_pkts + task->n_new_pkts, fill) < 0)
		return ;
	task->n_new_pkts += fill;
}

/* If pkt_count is enabled (i.e., it is not -1), sending a packet
   reduces the value. The function will return the number of packets
   that can be sent, with a maximum of max as provided by the
   argument. While the count is set to 0, token are thrown away to
   limit transmission rate after resume. */
static uint32_t task_gen_calc_count(struct task_gen *task, uint32_t max)
{
	if (task->pkt_count == 0) {
		token_time_reset(&task->token_time, rte_rdtsc(), 0);
		return 0;
	} else if (task->pkt_count != (uint32_t)-1) {
		return task->pkt_count <= max? task->pkt_count : max;
	}
	return max;
}

static void task_gen_take_count(struct task_gen *task, uint32_t send_bulk)
{
	if (task->pkt_count == (uint32_t)-1)
		return ;
	else {
		if (task->pkt_count >= send_bulk)
			task->pkt_count -= send_bulk;
		else
			task->pkt_count = 0;
	}
}

static void handle_gen_pcap_bulk(struct task_base *tbase, struct rte_mbuf **mbuf, uint16_t n_pkts)
{
	struct task_gen_pcap *task = (struct task_gen_pcap *)tbase;
	uint64_t now = rte_rdtsc();
	uint64_t send_bulk = 0;
	uint32_t pkt_idx_tmp = task->pkt_idx;

	if (pkt_idx_tmp == task->n_pkts) {
		PROX_ASSERT(task->loop);
		return;
	}

	for (uint16_t j = 0; j < 64; ++j) {
		uint64_t tsc = task->proto_tsc[pkt_idx_tmp];
		if (task->last_tsc + tsc <= now) {
			task->last_tsc += tsc;
			send_bulk++;
			pkt_idx_tmp++;
			if (pkt_idx_tmp == task->n_pkts) {
				if (task->loop)
					pkt_idx_tmp = 0;
				else
					break;
			}
		}
		else
			break;
	}

	if (task->n_new_pkts < send_bulk) {
		uint32_t fill = 64 - task->n_new_pkts;
		if (rte_mempool_get_bulk(task->mempool, (void **)task->new_pkts + task->n_new_pkts, fill) < 0)
			return ;
		task->n_new_pkts += fill;
	}
	struct rte_mbuf **new_pkts = task->new_pkts + task->n_new_pkts - send_bulk;

	for (uint16_t j = 0; j < send_bulk; ++j) {
		struct rte_mbuf *next_pkt = new_pkts[j];

		rte_pktmbuf_pkt_len(next_pkt) = task->proto[task->pkt_idx].len;
		rte_pktmbuf_data_len(next_pkt) = task->proto[task->pkt_idx].len;
		init_mbuf_seg(next_pkt);

		uint8_t *hdr = rte_pktmbuf_mtod(next_pkt, uint8_t *);
		rte_memcpy(hdr, task->proto[task->pkt_idx].buf, task->proto[task->pkt_idx].len);

		task->pkt_idx++;
		if (task->pkt_idx == task->n_pkts) {
			if (task->loop)
				task->pkt_idx = 0;
			else
				break;
		}
	}

	task->base.tx_pkt(&task->base, new_pkts, send_bulk, NULL);
	task->n_new_pkts -= send_bulk;
}

static uint64_t bytes_to_tsc(uint64_t hz, uint32_t bytes, uint64_t bytes_per_hz, uint8_t through_ring)
{
	if (through_ring)
		return 0;
	else
		return hz * bytes / bytes_per_hz;
}

static uint32_t task_gen_next_pkt_idx(struct task_gen *task, uint32_t pkt_idx)
{
	return pkt_idx + 1 == task->n_pkts? 0 : pkt_idx + 1;
}

static uint32_t task_gen_calc_send_bulk(struct task_gen *task, uint32_t max_bulk)
{
	uint32_t send_bulk = 0;
	uint32_t pkt_idx_tmp = task->pkt_idx;
	uint32_t would_send_bytes = 0;

	for (uint16_t j = 0; j < max_bulk; ++j) {
		uint32_t pkt_size = task->proto[pkt_idx_tmp].len;
		uint32_t pkt_len = pkt_len_to_wire_size(pkt_size);
		if (pkt_len + would_send_bytes > task->token_time.bytes_now)
			break;

		pkt_idx_tmp = task_gen_next_pkt_idx(task, pkt_idx_tmp);

		send_bulk++;
		would_send_bytes += pkt_len;
	}
	return send_bulk;
}

static void handle_gen_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_gen *task = (struct task_gen *)tbase;
	uint64_t accur, now = 0, offset_last_packet, diff = 0;

	/* next 2 values are passed empty by thread_call */
	(void)mbufs;
	(void)n_pkts;

	if (task->token_time.cfg.bpp != task->new_rate_bps)
		task_gen_reset_token_time(task);
	if (!task->token_time.cfg.bpp)
		return ;
	token_time_update(&task->token_time, rte_rdtsc());

	/* The biggest bulk we allow to send is task->max_bulk_size
	   packets. At the same time, we are rate limiting based on
	   the specified speed (in bytes per second). Furthermore, the
	   bulk is also limited by the pkt_count field.*/
	const uint32_t max_pkts = task_gen_calc_count(task, task->max_bulk_size);
	uint32_t send_bulk = task_gen_calc_send_bulk(task, max_pkts);

	/* Loop was too fast. */
	if (send_bulk < task->min_bulk_size)
		return ;
	if (task->n_new_pkts < send_bulk)
		task_gen_refill_new_pkts(task);
	task_gen_take_count(task, send_bulk);

	if (send_bulk > task->n_new_pkts)
		send_bulk = task->n_new_pkts;
	if (send_bulk == 0)
		return;
	uint64_t will_send_bytes = 0;
	struct rte_mbuf **new_pkts = task->new_pkts + task->n_new_pkts - send_bulk;
	uint32_t pkt_len = 0;
	uint8_t *pkt_hdr[MAX_RING_BURST];

	for (uint16_t j = 0; j < send_bulk; ++j) {
		rte_prefetch0(new_pkts[j]);
	}
	for (uint16_t j = 0; j < send_bulk; ++j) {
		struct rte_mbuf *next_pkt = new_pkts[j];
		pkt_hdr[j] = rte_pktmbuf_mtod(next_pkt, uint8_t *);
	}
	for (uint16_t j = 0; j < send_bulk; ++j) {
		rte_prefetch0(pkt_hdr[j]);
	}

	for (uint16_t j = 0; j < send_bulk; ++j) {
		uint32_t pkt_size = task->proto[task->pkt_idx].len;
		struct rte_mbuf *next_pkt = new_pkts[j];
		pkt_len = pkt_len_to_wire_size(pkt_size);

		rte_pktmbuf_pkt_len(next_pkt) = pkt_size;
		rte_pktmbuf_data_len(next_pkt) = pkt_size;
		init_mbuf_seg(next_pkt);

		uint8_t *hdr = pkt_hdr[j];
		rte_memcpy(hdr, task->proto[task->pkt_idx].buf, task->proto[task->pkt_idx].len);

		if (task->lat_enabled) {
			task->pkt_tsc[j].tsc_offset = bytes_to_tsc(task->hz, will_send_bytes, 1250000000, task->through_ring);
			task->pkt_tsc[j].pos = (uint32_t *)(hdr + task->lat_pos);
		}
		will_send_bytes += pkt_len;
		task->pkt_idx = task_gen_next_pkt_idx(task, task->pkt_idx);
	}
	if (task->n_rands) {
		for (uint16_t j = 0; j < send_bulk; ++j) {
			set_random_fields(task, pkt_hdr[j]);
		}
	}
	if (task->n_values) {
		for (uint16_t j = 0; j < send_bulk; ++j) {
			set_fixed_fields(task, pkt_hdr[j]);
		}
	}
	if (task->accur_pos) {
		for (uint16_t j = 0; j < send_bulk; ++j) {
			/* Here, the accuracy of task->pkt_queue_index
			   - 64 is stored in packet
			   task->pkt_queue_index. The ID modulo 64 is
			   the same. */
			*(uint32_t *)(pkt_hdr[j] + task->accur_pos) = task->accur[(task->pkt_queue_index + j) & 63];
		}
	}
	if (task->packet_id_pos) {
		for (uint16_t j = 0; j < send_bulk; ++j) {
			*(uint8_t *)(pkt_hdr[j] + task->packet_id_pos) = task->queue_id;
		}
		for (uint16_t j = 0; j < send_bulk; ++j) {
			*(uint32_t *)(pkt_hdr[j] + task->packet_id_pos + 1) = task->pkt_queue_index++;
		}
	}
	if (task->runtime_flags & TASK_TX_CRC) {
		for (uint16_t j = 0; j < send_bulk; ++j) {
			checksum_packet((struct ether_hdr *)pkt_hdr[j], new_pkts[j]);
		}
	}

	offset_last_packet = bytes_to_tsc(task->hz, pkt_len, 1250000000, task->through_ring);
	/* If max burst has been sent, we can't keep up so just assume
	   that we can (leaving a "gap" in the packet stream on the
	   wire) */
	task->token_time.bytes_now -= will_send_bytes;
	if ((send_bulk == task->max_bulk_size) && (task->token_time.bytes_now > will_send_bytes)) {
		task->token_time.bytes_now = will_send_bytes;
	}

	/* Just before sending the packets, apply the time stamp
	   relative to when the first packet will be sent. The first
	   packet will be sent now. The time is read for each packet
	   to reduce the error towards the actual time the packet will
	   be sent. */

	if (task->lat_enabled) {
		accur = rte_rdtsc();
		/* assume that the time it took to write the tsc
		   previously is a good estimator for the time it will
		   take now.. */
		uint64_t beg = accur;
		now = beg + task->write_tsc;

		if (now < task->time_next_pkt) {
			// We are ready to send packets while last packet not sent yet
			// We cannot write a timestamp in the past
			diff = task->time_next_pkt - now;
			now = task->time_next_pkt;
		}
		for (uint16_t j = 0; j < send_bulk; ++j) {
			*(task->pkt_tsc[j].pos) = (now + task->pkt_tsc[j].tsc_offset) >> LATENCY_ACCURACY;
		}
		task->time_next_pkt = now + task->pkt_tsc[send_bulk-1].tsc_offset + offset_last_packet;
		task->write_tsc = rte_rdtsc() - beg;

		/* Make sure that the time stamps that were written
		   are valid. */
		while (rte_rdtsc() + diff < now);
	}

	task->base.tx_pkt(&task->base, new_pkts, send_bulk, NULL);
	task->n_new_pkts -= send_bulk;

	if (task->accur_pos) {
		accur = rte_rdtsc() - now + diff;
		for (uint64_t i = task->pkt_queue_index - send_bulk; i < task->pkt_queue_index; ++i) {
			task->accur[i & 63] = accur;
		}
	}
}

static void init_task_gen_seeds(struct task_gen *task)
{
	for (size_t i = 0; i < sizeof(task->rand)/sizeof(task->rand[0]); ++i)
		task->rand[i].seeds = rte_rdtsc();
}

static uint32_t pcap_count_pkts(pcap_t *handle)
{
	struct pcap_pkthdr header;
	const uint8_t *buf;
	uint32_t ret = 0;
	long pkt1_fpos = ftell(pcap_file(handle));

	while ((buf = pcap_next(handle, &header))) {
		ret++;
	}
	int ret2 = fseek(pcap_file(handle), pkt1_fpos, SEEK_SET);
	PROX_PANIC(ret2 != 0, "Failed to reset reading pcap file\n");
	return ret;
}

static uint64_t avg_time_stamp(uint64_t *time_stamp, uint32_t n)
{
	uint64_t tot_inter_pkt = 0;

	for (uint32_t i = 0; i < n; ++i)
		tot_inter_pkt += time_stamp[i];
	return (tot_inter_pkt + n / 2)/n;
}

static int pcap_read_pkts(pcap_t *handle, const char *file_name, uint32_t n_pkts, struct gen_proto *proto, uint64_t *time_stamp)
{
	struct pcap_pkthdr header;
	const uint8_t *buf;
	size_t len;

	for (uint32_t i = 0; i < n_pkts; ++i) {
		buf = pcap_next(handle, &header);

		PROX_PANIC(buf == NULL, "Failed to read packet %d from pcap %s\n", i, file_name);
		proto[i].len = header.len;
		len = RTE_MIN(header.len, sizeof(proto[i].buf));
		if (header.len > len)
			plogx_warn("Packet truncated from %u to %zu bytes\n", header.len, len);

		if (time_stamp) {
			static struct timeval beg;
			struct timeval tv;

			if (i == 0)
				beg = header.ts;

			tv = tv_diff(&beg, &header.ts);
			tv_to_tsc(&tv, time_stamp + i);
		}
		rte_memcpy(proto[i].buf, buf, len);
	}

	if (time_stamp && n_pkts) {
		for (uint32_t i = n_pkts - 1; i > 0; --i)
			time_stamp[i] -= time_stamp[i - 1];
		/* Since the handle function will loop the packets,
		   there is one time-stamp that is not provided by the
		   pcap file. This is the time between the last and
		   the first packet. This implementation takes the
		   average of the inter-packet times here. */
		if (n_pkts > 1)
			time_stamp[0] = avg_time_stamp(time_stamp + 1, n_pkts - 1);
	}

	return 0;
}

static void check_length(struct task_gen *task, uint32_t pkt_size)
{
	const uint16_t min_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
	const uint16_t max_len = ETHER_MAX_LEN - 4;

	PROX_PANIC((pkt_size > max_len), "pkt_size out of range (must be <= %u)\n", max_len);
	PROX_PANIC((pkt_size < min_len), "pkt_size out of range (must be >= %u)\n", min_len);
	PROX_PANIC((task->lat_enabled) && (pkt_size < task->lat_pos + 4U), "Writing latency outside the packet\n");
	PROX_PANIC((task->packet_id_pos) && (pkt_size < task->packet_id_pos + 5U), "Writing packet id outside the packet\n");
	PROX_PANIC((task->accur_pos) && (pkt_size < task->accur_pos + 4U), "Writing accur outside the packet\n");
}

static void task_init_gen_load_pkt_inline(struct task_gen *task, struct task_args *targ)
{
	const int socket_id = rte_lcore_to_socket_id(targ->lconf->id);

	task->n_pkts = 1;
	PROX_PANIC(targ->pkt_size == 0, "Invalid packet size length (no packet defined?)\n");
	check_length(task, targ->pkt_size);

	task->proto = prox_zmalloc(task->n_pkts * sizeof(*task->proto), socket_id);
	rte_memcpy(task->proto[0].buf, targ->pkt_inline, RTE_MIN(targ->pkt_size, sizeof(task->proto[0].buf)));
	task->proto[0].len = targ->pkt_size;
}

static void task_init_gen_load_pcap(struct task_gen *task, struct task_args *targ)
{
	const int socket_id = rte_lcore_to_socket_id(targ->lconf->id);
	char err[PCAP_ERRBUF_SIZE];
	pcap_t *handle = pcap_open_offline(targ->pcap_file, err);
	PROX_PANIC(handle == NULL, "Failed to open PCAP file: %s\n", err);

	task->n_pkts = pcap_count_pkts(handle);
	plogx_info("%u packets in pcap file '%s'\n", task->n_pkts, targ->pcap_file);

	if (targ->n_pkts)
		task->n_pkts = RTE_MIN(task->n_pkts, targ->n_pkts);
	plogx_info("Loading %u packets from pcap\n", task->n_pkts);
	task->proto = prox_zmalloc(task->n_pkts * sizeof(*task->proto), socket_id);
	PROX_PANIC(task->proto == NULL, "Failed to allocate %lu bytes (in huge pages) for pcap file\n", task->n_pkts * sizeof(*task->proto));

	pcap_read_pkts(handle, targ->pcap_file, task->n_pkts, task->proto, NULL);
	pcap_close(handle);
}

static struct rte_mempool *task_gen_create_mempool(struct task_args *targ)
{
	static char name[] = "gen_pool";
	struct rte_mempool *ret;
	const int sock_id = rte_lcore_to_socket_id(targ->lconf->id);

	name[0]++;
	ret = rte_mempool_create(name, targ->nb_mbuf - 1, MBUF_SIZE,
				 targ->nb_cache_mbuf, sizeof(struct rte_pktmbuf_pool_private),
				 rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, 0,
				 sock_id, 0);
	PROX_PANIC(ret == NULL, "Failed to allocate dummy memory pool on socket %u with %u elements\n",
		   sock_id, targ->nb_mbuf - 1);
	return ret;
}

void task_gen_set_pkt_count(struct task_base *tbase, uint32_t count)
{
	struct task_gen *task = (struct task_gen *)tbase;

	task->pkt_count = count;
}

void task_gen_set_pkt_size(struct task_base *tbase, uint32_t pkt_size)
{
	struct task_gen *task = (struct task_gen *)tbase;

	check_length(task, pkt_size);
	task->proto[0].len = pkt_size;
}

void task_gen_set_rate(struct task_base *tbase, uint64_t bps)
{
	struct task_gen *task = (struct task_gen *)tbase;

	task->new_rate_bps = bps;
}

void task_gen_reset_randoms(struct task_base *tbase)
{
	struct task_gen *task = (struct task_gen *)tbase;

	for (uint32_t i = 0; i < task->n_rands; ++i) {
		task->rand[i].rand_mask = 0;
		task->rand[i].fixed_bits = 0;
		task->rand[i].rand_offset = 0;
	}
	task->n_rands = 0;
}

int task_gen_set_value(struct task_base *tbase, uint32_t value, uint32_t offset, uint32_t len)
{
	struct task_gen *task = (struct task_gen *)tbase;

	if (task->n_values >= 64)
		return -1;

	task->fixed[task->n_values].value = rte_cpu_to_be_32(value) >> ((4 - len) * 8);
	task->fixed[task->n_values].offset = offset;
	task->fixed[task->n_values].value_len = len;
	task->n_values++;

	return 0;
}

void task_gen_reset_values(struct task_base *tbase)
{
	struct task_gen *task = (struct task_gen *)tbase;

	task->n_values = 0;
}

uint32_t task_gen_get_n_randoms(struct task_base *tbase)
{
	struct task_gen *task = (struct task_gen *)tbase;

	return task->n_rands;
}

uint32_t task_gen_get_n_values(struct task_base *tbase)
{
	struct task_gen *task = (struct task_gen *)tbase;

	return task->n_values;
}

static void init_task_gen_pcap(struct task_base *tbase, struct task_args *targ)
{
	struct task_gen_pcap *task = (struct task_gen_pcap *)tbase;
	const uint32_t sockid = rte_lcore_to_socket_id(targ->lconf->id);

	task->loop = targ->loop;
	task->pkt_idx = 0;
	task->hz = rte_get_tsc_hz();
	task->mempool = task_gen_create_mempool(targ);

	PROX_PANIC(!strcmp(targ->pcap_file, ""), "No pcap file defined\n");

	char err[PCAP_ERRBUF_SIZE];
	pcap_t *handle = pcap_open_offline(targ->pcap_file, err);
	PROX_PANIC(handle == NULL, "Failed to open PCAP file: %s\n", err);

	task->n_pkts = pcap_count_pkts(handle);
	plogx_info("%u packets in pcap file '%s'\n", task->n_pkts, targ->pcap_file);

	if (targ->n_pkts) {
		plogx_info("Configured to load %u packets\n", targ->n_pkts);
		if (task->n_pkts > targ->n_pkts)
			task->n_pkts = targ->n_pkts;
	}

	plogx_info("Loading %u packets from pcap\n", task->n_pkts);

	size_t mem_size = task->n_pkts * (sizeof(*task->proto) + sizeof(*task->proto_tsc));
	uint8_t *mem = prox_zmalloc(mem_size, sockid);

	PROX_PANIC(mem == NULL, "Failed to allocate %lu bytes (in huge pages) for pcap file\n", mem_size);
	task->proto = (struct gen_proto *) mem;
	task->proto_tsc = (uint64_t *)(mem + task->n_pkts * sizeof(*task->proto));

	pcap_read_pkts(handle, targ->pcap_file, task->n_pkts, task->proto, task->proto_tsc);
	pcap_close(handle);
}

static int task_gen_find_random_with_offset(struct task_gen *task, uint32_t offset)
{
	for (uint32_t i = 0; i < task->n_rands; ++i) {
		if (task->rand[i].rand_offset == offset) {
			return i;
		}
	}

	return UINT32_MAX;
}

int task_gen_add_rand(struct task_base *tbase, const char *rand_str, uint32_t offset, uint32_t rand_id)
{
	struct task_gen *task = (struct task_gen *)tbase;
	uint32_t existing_rand;

	if (rand_id == UINT32_MAX && task->n_rands == 64) {
		plog_err("Too many randoms\n");
		return -1;
	}
	uint32_t mask, fixed, len;

	if (parse_random_str(&mask, &fixed, &len, rand_str)) {
		plog_err("%s\n", get_parse_err());
		return -1;
	}

	existing_rand = task_gen_find_random_with_offset(task, offset);
	if (existing_rand != UINT32_MAX) {
		plog_warn("Random at offset %d already set => overwriting len = %d %sg\n", offset, len, rand_str);
		rand_id = existing_rand;
	}

	task->rand[task->n_rands].rand_len = len;
	task->rand[task->n_rands].rand_offset = offset;
	task->rand[task->n_rands].rand_mask = mask;
	task->rand[task->n_rands].fixed_bits = fixed;

	if ((task->rand[task->n_rands].rand_mask & RAND_MAX) != task->rand[task->n_rands].rand_mask) {
		plog_err("Using rand() as random generator\n"
			 "Generated values with rand() are in the range [0, %#08x].\n"
			 "The provided mask was %#08x. Suggesting to use 2 random fields instead.\n"
			 "The provided random string was '%s'\n",
			 RAND_MAX, task->rand[task->n_rands].rand_mask, rand_str);
		return -1;
	}
	task->n_rands++;
	return 0;
}

static void init_task_gen(struct task_base *tbase, struct task_args *targ)
{
	struct task_gen *task = (struct task_gen *)tbase;

	task->packet_id_pos = targ->packet_id_pos;

	task->mempool = task_gen_create_mempool(targ);
	PROX_PANIC(task->mempool == NULL, "Failed to create mempool\n");
	task->pkt_idx = 0;
	task->hz = rte_get_tsc_hz();
	task->lat_pos = targ->lat_pos;
	task->accur_pos = targ->accur_pos;
	task->new_rate_bps = targ->rate_bps;

	struct token_time_cfg tt_cfg = token_time_cfg_create(1250000000, rte_get_tsc_hz(), -1);

	token_time_init(&task->token_time, &tt_cfg);
	init_task_gen_seeds(task);

	task->min_bulk_size = targ->min_bulk_size;
	task->max_bulk_size = targ->max_bulk_size;
	if (task->min_bulk_size < 1)
		task->min_bulk_size = 1;
	if (task->max_bulk_size < 1)
		task->max_bulk_size = 64;
	PROX_PANIC(task->max_bulk_size > 64, "max_bulk_size higher than 64\n");
	PROX_PANIC(task->max_bulk_size < task->min_bulk_size, "max_bulk_size must be > than min_bulk_size\n");

	for (uint32_t i = 0; i < targ->n_rand_str; ++i) {
		PROX_PANIC(task_gen_add_rand(tbase, targ->rand_str[i], targ->rand_offset[i], UINT32_MAX),
			   "Failed to add random\n");
	}

	task->pkt_count = -1;
	task->lat_enabled = targ->lat_enabled;
	task->runtime_flags = targ->runtime_flags;
	PROX_PANIC((task->lat_pos || task->accur_pos) && !(task->lat_enabled), "lat not enabled by lat pos or accur pos configured\n");
	if ((targ->nb_txrings == 0) && (targ->nb_txports == 1)) {
		task->queue_id = tbase->tx_params_hw.tx_port_queue[0].queue;
	} else if ((targ->nb_txrings == 1) && (targ->nb_txports == 0)) {
		task->queue_id = 0;
		task->through_ring = 1;
	} else {
		PROX_PANIC(1, "Unexpected configuration with %d rings and %d ports\n", targ->nb_txrings, targ->nb_txports);
	}

	if (!strcmp(targ->pcap_file, "")) {
		plog_info("Using inline definition of a packet\n");
		task_init_gen_load_pkt_inline(task, targ);
	} else {
		plog_info("Loading from pcap %s\n", targ->pcap_file);
		task_init_gen_load_pcap(task, targ);
	}

	if ((targ->flags & DSF_KEEP_SRC_MAC) == 0) {
		uint8_t *src_addr = prox_port_cfg[tbase->tx_params_hw.tx_port_queue->port].eth_addr.addr_bytes;
		for (uint32_t i = 0; i < task->n_pkts; ++i) {
			rte_memcpy(&task->proto[i].buf[6], src_addr, 6);
		}
	}
}

static struct task_init task_init_gen = {
	.mode_str = "gen",
	.init = init_task_gen,
	.handle = handle_gen_bulk,
	.start = start,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS | TASK_FEATURE_NO_RX,
	.size = sizeof(struct task_gen)
};

static struct task_init task_init_gen_pcap = {
	.mode_str = "gen",
	.sub_mode_str = "pcap",
	.init = init_task_gen_pcap,
	.handle = handle_gen_pcap_bulk,
	.start = start_pcap,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS | TASK_FEATURE_NO_RX,
	.size = sizeof(struct task_gen_pcap)
};

__attribute__((constructor)) static void reg_task_gen(void)
{
	reg_task(&task_init_gen);
	reg_task(&task_init_gen_pcap);
}
