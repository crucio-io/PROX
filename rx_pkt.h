/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2017 Viosoft Corporation.
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

#ifndef _RX_PKT_H_
#define _RX_PKT_H_

#include <inttypes.h>

struct rte_mbuf;
struct task_base;

uint16_t rx_pkt_hw(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_hw_pow2(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_hw1(struct task_base *tbase, struct rte_mbuf ***mbufs);

/* The _multi variation of the function is used to work-around the
   problem with QoS, multi-seg mbufs and vector PMD. When vector
   PMD returns more than 32 packets, the two variations of the
   receive function can be merged back together. */
uint16_t rx_pkt_hw_multi(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_hw_pow2_multi(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_hw1_multi(struct task_base *tbase, struct rte_mbuf ***mbufs);

uint16_t rx_pkt_sw(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_sw_pow2(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_sw1(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_self(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_dummy(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_dump(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_trace(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_distr(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_bw(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_tsc(struct task_base *tbase, struct rte_mbuf ***mbufs);
uint16_t rx_pkt_all(struct task_base *tbase, struct rte_mbuf ***mbufs);

#endif /* _RX_PKT_H_ */
