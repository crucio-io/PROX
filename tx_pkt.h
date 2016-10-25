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

#ifndef _TX_PKT_H_
#define _TX_PKT_H_

#include <inttypes.h>

struct task_base;
struct rte_mbuf;

void flush_queues_hw(struct task_base *tbase);
void flush_queues_sw(struct task_base *tbase);

void flush_queues_no_drop_hw(struct task_base *tbase);
void flush_queues_no_drop_sw(struct task_base *tbase);

/* The following four transmit functions always send packets to the
   single output unless the packet should be dropped. These functions
   are used if (1) the task is only sending to one destination and
   (2), packets can potentially be dropped (as specified by the out
   parameter, which is either NO_PORT_AVAIL or 0). */
void tx_pkt_no_drop_hw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_no_drop_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_hw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);

/* The following four transmit functions are used if (1) the task is
   only sending to one destination and (2), packets are never dropped
   by the task (the out parameter is ignored). */
void tx_pkt_no_drop_never_discard_hw1_lat_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_no_drop_never_discard_hw1_thrpt_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_no_drop_never_discard_hw1_no_pointer(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_no_drop_never_discard_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_never_discard_hw1_lat_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_never_discard_hw1_thrpt_opt(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_never_discard_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);

/* The two "self" transmit functions are used if the task is
   transmitting to another task running on the same core and the
   destination task ID is one higher than the current task. The never_discard
   version of the function ignores the out parameter and should
   therefor only be used if the task never discards packets.*/
void tx_pkt_self(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
void tx_pkt_never_discard_self(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);

/* The following four tarnsmit functions are the most general. They
   are used if (1) packets can be dropped and (2) there are multiple
   outputs in the task. */
void tx_pkt_no_drop_hw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);
void tx_pkt_no_drop_sw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);
void tx_pkt_hw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);
void tx_pkt_sw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);

void tx_pkt_trace(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);
void tx_pkt_dump(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);
void tx_pkt_distr(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);
void tx_pkt_bw(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);

uint16_t tx_try_sw1(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts);
uint16_t tx_try_hw1(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts);
uint16_t tx_try_self(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts);

/* When there are no output ports, this function is configured as the
   tx function. This tx function can be used to make each task a
   sink. */
void tx_pkt_drop_all(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, uint8_t *out);

#endif /* _TX_PKT_H_ */
