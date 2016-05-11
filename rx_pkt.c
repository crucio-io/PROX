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

#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_version.h>

#include "rx_pkt.h"
#include "task_base.h"
#include "clock.h"
#include "stats.h"
#include "log.h"
#include "mbuf_utils.h"
#include "input.h" /* Needed for callback on dump */

/* _param version of the rx_pkt_hw functions are used to create two
   instances of very similar variations of these functions. The
   variations are specified by the "twice" parameter which significies
   that the rte_eth_rx_burst function should be called twice. The
   reason for this is that with the vector PMD, the maximum number of
   packets being returned is 32 and some algorithms (like QoS) only
   work correctly if more than 32 packets are received if the dequeue
   step involves finding 32 packets. */
static uint16_t rx_pkt_hw_param(struct task_base *tbase, struct rte_mbuf ***mbufs, int twice)
{
	uint8_t last_read_portid;
	uint16_t nb_rx;

	START_EMPTY_MEASSURE();
	*mbufs = tbase->ws_mbuf->mbuf[0] +
		(RTE_ALIGN_CEIL(tbase->ws_mbuf->idx[0].prod, 2) & WS_MBUF_MASK);

	last_read_portid = tbase->rx_params_hw.last_read_portid;
	nb_rx = rte_eth_rx_burst(tbase->rx_params_hw.rx_pq[last_read_portid].port,
				 tbase->rx_params_hw.rx_pq[last_read_portid].queue,
				 *mbufs, MAX_PKT_BURST);

	if (twice) {
		if (nb_rx == 32) {
			nb_rx += rte_eth_rx_burst(tbase->rx_params_hw.rx_pq[last_read_portid].port,
				 tbase->rx_params_hw.rx_pq[last_read_portid].queue,
				 *mbufs + 32, MAX_PKT_BURST);
		}
	}

	++tbase->rx_params_hw.last_read_portid;
	if (unlikely(tbase->rx_params_hw.last_read_portid == tbase->rx_params_hw.nb_rxports)) {
		tbase->rx_params_hw.last_read_portid = 0;
	}
	if (likely(nb_rx > 0)) {
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
	return 0;
}

static uint16_t rx_pkt_hw_pow2_param(struct task_base *tbase, struct rte_mbuf ***mbufs, int twice)
{
	uint8_t lr;
	uint16_t nb_rx;

	START_EMPTY_MEASSURE();
	*mbufs = tbase->ws_mbuf->mbuf[0] +
		(RTE_ALIGN_CEIL(tbase->ws_mbuf->idx[0].prod, 2) & WS_MBUF_MASK);
	lr = tbase->rx_params_hw.last_read_portid;
	nb_rx = rte_eth_rx_burst(tbase->rx_params_hw.rx_pq[lr].port,
				 tbase->rx_params_hw.rx_pq[lr].queue,
				 *mbufs, MAX_PKT_BURST);

	if (twice) {
		if (nb_rx == 32) {
			nb_rx += rte_eth_rx_burst(tbase->rx_params_hw.rx_pq[lr].port,
						  tbase->rx_params_hw.rx_pq[lr].queue,
						  *mbufs + 32, MAX_PKT_BURST);
		}
	}

	tbase->rx_params_hw.last_read_portid = (lr + 1) & tbase->rx_params_hw.rxport_mask;

	if (likely(nb_rx > 0)) {
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
	return 0;
}

static inline uint16_t rx_pkt_hw1_param(struct task_base *tbase, struct rte_mbuf ***mbufs, int twice)
{
	uint16_t nb_rx;

	START_EMPTY_MEASSURE();
	*mbufs = tbase->ws_mbuf->mbuf[0] +
		(RTE_ALIGN_CEIL(tbase->ws_mbuf->idx[0].prod, 2) & WS_MBUF_MASK);

	nb_rx = rte_eth_rx_burst(tbase->rx_params_hw1.rx_pq.port,
				 tbase->rx_params_hw1.rx_pq.queue,
				 *mbufs, MAX_PKT_BURST);

	if (twice) {
		if (nb_rx == 32) {
			nb_rx += rte_eth_rx_burst(tbase->rx_params_hw1.rx_pq.port,
				 tbase->rx_params_hw1.rx_pq.queue,
				 *mbufs + 32, MAX_PKT_BURST);
		}
	}

	if (likely(nb_rx > 0)) {
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
	return 0;
}

uint16_t rx_pkt_hw(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	return rx_pkt_hw_param(tbase, mbufs, 0);
}

uint16_t rx_pkt_hw_pow2(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	return rx_pkt_hw_pow2_param(tbase, mbufs, 0);
}

uint16_t rx_pkt_hw1(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	return rx_pkt_hw1_param(tbase, mbufs, 0);
}

uint16_t rx_pkt_hw_twice(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	return rx_pkt_hw_param(tbase, mbufs, 1);
}

uint16_t rx_pkt_hw_pow2_twice(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	return rx_pkt_hw_pow2_param(tbase, mbufs, 1);
}

uint16_t rx_pkt_hw1_twice(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	return rx_pkt_hw1_param(tbase, mbufs, 1);
}

/* The following functions implement ring access */
static uint16_t ring_deq(struct rte_ring *r, struct rte_mbuf **mbufs)
{
	void **v_mbufs = (void **)mbufs;
#ifdef BRAS_RX_BULK
	return rte_ring_sc_dequeue_bulk(r, v_mbufs, MAX_RING_BURST) < 0? 0 : MAX_RING_BURST;
#else
	return rte_ring_sc_dequeue_burst(r, v_mbufs, MAX_RING_BURST);
#endif
}

uint16_t rx_pkt_sw(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	START_EMPTY_MEASSURE();
	*mbufs = tbase->ws_mbuf->mbuf[0] + (tbase->ws_mbuf->idx[0].prod & WS_MBUF_MASK);
	uint8_t lr = tbase->rx_params_sw.last_read_ring;
	uint16_t nb_rx;

	do {
		nb_rx = ring_deq(tbase->rx_params_sw.rx_rings[lr], *mbufs);
		lr = lr + 1 == tbase->rx_params_sw.nb_rxrings? 0 : lr + 1;
	} while(!nb_rx && lr != tbase->rx_params_sw.last_read_ring);

	tbase->rx_params_sw.last_read_ring = lr;

	if (nb_rx != 0) {
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	else {
		TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
		return 0;
	}
}

/* Same as rx_pkt_sw expect with a mask for the number of receive
   rings (can only be used if nb_rxring is a power of 2). */
uint16_t rx_pkt_sw_pow2(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	START_EMPTY_MEASSURE();
	*mbufs = tbase->ws_mbuf->mbuf[0] + (tbase->ws_mbuf->idx[0].prod & WS_MBUF_MASK);
	uint8_t lr = tbase->rx_params_sw.last_read_ring;
	uint16_t nb_rx;

	do {
		nb_rx = ring_deq(tbase->rx_params_sw.rx_rings[lr], *mbufs);
		lr = (lr + 1) & tbase->rx_params_sw.rxrings_mask;
	} while(!nb_rx && lr != tbase->rx_params_sw.last_read_ring);

	tbase->rx_params_sw.last_read_ring = lr;

	if (nb_rx != 0) {
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	else {
		TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
		return 0;
	}
}

uint16_t rx_pkt_self(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	START_EMPTY_MEASSURE();
	uint16_t nb_rx = tbase->ws_mbuf->idx[0].nb_rx;
	if (nb_rx) {
		tbase->ws_mbuf->idx[0].nb_rx = 0;
		*mbufs = tbase->ws_mbuf->mbuf[0] + (tbase->ws_mbuf->idx[0].prod & WS_MBUF_MASK);
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	else {
		TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
		return 0;
	}
}

/* Used for tasks that do not receive packets (i.e. Packet
generation).  Always returns 1 but never returns packets and does not
increment statistics. This function allows to use the same code path
as for tasks that actually receive packets. */
uint16_t rx_pkt_dummy(__attribute__((unused)) struct task_base *tbase,
		      __attribute__((unused)) struct rte_mbuf ***mbufs)
{
	return 1;
}

/* After the system has been configured, it is known if there is only
   one RX ring. If this is the case, a more specialized version of the
   function above can be used to save cycles. */
uint16_t rx_pkt_sw1(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	START_EMPTY_MEASSURE();
	*mbufs = tbase->ws_mbuf->mbuf[0] + (tbase->ws_mbuf->idx[0].prod & WS_MBUF_MASK);
	uint16_t nb_rx = ring_deq(tbase->rx_params_sw1.rx_ring, *mbufs);

	if (nb_rx != 0) {
		TASK_STATS_ADD_RX(&tbase->aux->stats, nb_rx);
		return nb_rx;
	}
	else {
		TASK_STATS_ADD_IDLE(&tbase->aux->stats, rte_rdtsc() - cur_tsc);
		return 0;
	}
}

/* Only used when there are packets to be dumped. This function is
   meant as a debugging tool and is therefore not optimized. When the
   number of packets to dump falls back to 0, the original (optimized)
   rx function is restored. This allows to support dumping packets
   without any performance impact if the feature is not used. */
uint16_t rx_pkt_dump(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	uint16_t ret = tbase->aux->rx_pkt_orig(tbase, mbufs);

	if (ret) {
		uint32_t n_dump = tbase->aux->task_rt_dump.n_print_rx;
		n_dump = ret < n_dump? ret : n_dump;

		if (tbase->aux->task_rt_dump.input->reply == NULL) {
			for (uint32_t i = 0; i < n_dump; ++i) {
				plogd_info((*mbufs)[i], "RX: ");
			}
		}
		else {
			struct input *input = tbase->aux->task_rt_dump.input;

			for (uint32_t i = 0; i < n_dump; ++i) {
				/* TODO: Execute callback with full
				   data in a single call. */
				char tmp[128];
				int strlen;

#if RTE_VERSION >= RTE_VERSION_NUM(1,8,0,0)
				int port_id = ((*mbufs)[i])->port;
#else
				int port_id = ((*mbufs)[i])->pkt.in_port;
#endif
				strlen = snprintf(tmp, sizeof(tmp), "pktdump,%d,%d\n", port_id,
						      rte_pktmbuf_pkt_len((*mbufs)[i]));

				input->reply(input, tmp, strlen);
				input->reply(input, rte_pktmbuf_mtod((*mbufs)[i], char *), rte_pktmbuf_pkt_len((*mbufs)[i]));
				input->reply(input, "\n", 1);
			}
		}

		tbase->aux->task_rt_dump.n_print_rx -= n_dump;

		if (0 == tbase->aux->task_rt_dump.n_print_rx) {
			tbase->rx_pkt = tbase->aux->rx_pkt_orig;
			tbase->aux->rx_pkt_orig = NULL;
		}
	}
	return ret;
}

uint16_t rx_pkt_trace(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	uint16_t ret = tbase->aux->rx_pkt_orig(tbase, mbufs);

	if (ret) {
		uint32_t n_trace = tbase->aux->task_rt_dump.n_trace;
		n_trace = ret < n_trace? ret : n_trace;
		tbase->aux->task_rt_dump.cur_trace = n_trace;

		for (uint32_t i = 0; i < n_trace; ++i) {
			uint8_t *pkt = rte_pktmbuf_mtod((*mbufs)[i], uint8_t *);
			rte_memcpy(tbase->aux->task_rt_dump.pkt_cpy, pkt, sizeof(tbase->aux->task_rt_dump.pkt_cpy[i]));
			tbase->aux->task_rt_dump.pkt_cpy_len[i] = rte_pktmbuf_pkt_len((*mbufs)[i]);
			tbase->aux->task_rt_dump.pkt_mbuf_addr[i] = (*mbufs)[i];
		}

		tbase->aux->task_rt_dump.n_trace -= n_trace;
		/* Unset by TX when n_trace = 0 */
	}
	return ret;
}

/* Gather the distribution of the number of packets that have been
   received from one RX call. Since the value is only modified by the
   task that receives the packet, no atomic operation is needed. */
uint16_t rx_pkt_distr(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	uint16_t ret = tbase->aux->rx_pkt_orig(tbase, mbufs);

	tbase->aux->rx_bucket[ret]++;
	return ret;
}

uint16_t rx_pkt_bw(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	uint16_t ret = tbase->aux->rx_pkt_orig(tbase, mbufs);
	uint32_t tot_bytes = 0;

	for (uint16_t i = 0; i < ret; ++i) {
		tot_bytes += mbuf_wire_size((*mbufs)[i]);
	}

	TASK_STATS_ADD_RX_BYTES(&tbase->aux->stats, tot_bytes);

	return ret;
}

uint16_t rx_pkt_tsc(struct task_base *tbase, struct rte_mbuf ***mbufs)
{
	uint64_t before = rte_rdtsc();
	uint16_t ret = tbase->aux->tsc_rx.rx_pkt_orig(tbase, mbufs);

	if (unlikely(ret == 0)) {
		tbase->aux->tsc_rx.begin = before;
	} else {
		uint64_t after = rte_rdtsc();
		tbase->aux->tsc_rx.before = before;
		tbase->aux->tsc_rx.after = after;
	}
	return ret;
}
