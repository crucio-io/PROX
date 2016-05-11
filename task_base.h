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

#ifndef _TASK_BASE_H_
#define _TASK_BASE_H_

#include <rte_common.h>
#ifndef __rte_cache_aligned
#include <rte_memory.h>
#endif

#include "defaults.h"
#include "prox_globals.h"
#include "stats_task.h"

// runtime_flags 8 bits only
#define TASK_MPLS_TAGGING              0x0001
#define TASK_ROUTING                   0x0002
#define TASK_CLASSIFY                  0x0004
#define TASK_CTRL_HANDLE_ARP           0x0008
#define TASK_MARK                      0x0020
#define TASK_FP_HANDLE_ARP             0x0040
#define TASK_TX_CRC                    0x0080

// flag_features 32 bits
#define TASK_FEATURE_ROUTING           0x0001
#define TASK_FEATURE_CLASSIFY          0x0002
#define TASK_FEATURE_TWICE_RX                  0x0004
#define TASK_FEATURE_NEVER_DISCARDS            0x0008
#define TASK_FEATURE_NO_RX                     0x0010
#define TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS      0x0020
#define TASK_FEATURE_TXQ_FLAGS_NOMULTSEGS      0x0040
#define TASK_FEATURE_ZERO_RX                   0x0080
#define TASK_FEATURE_TXQ_FLAGS_REFCOUNT        0x0100
#define TASK_FEATURE_TSC_RX                    0x0200
#define TASK_FEATURE_THROUGHPUT_OPT            0x0400
#define TASK_FEATURE_GRE_ID                    0x1000
#define TASK_FEATURE_LUT_QINQ_RSS              0x2000
#define TASK_FEATURE_LUT_QINQ_HASH             0x4000

#define FLAG_TX_FLUSH                  0x01
#define FLAG_NEVER_FLUSH               0x02
// Task specific flags
#define BASE_FLAG_LUT_QINQ_HASH       	0x08
#define BASE_FLAG_LUT_QINQ_RSS       	0x10

#define OUT_DISCARD 0xFF
#define OUT_HANDLED 0xFE

#define WS_MBUF_MASK (2 * MAX_PKT_BURST - 1)

/* struct ws_mbuf stores the working set of mbufs. It starts with a
   prod/cons index to keep track of the number of elemenets. */
struct ws_mbuf {
	struct {
		uint16_t        prod;
		uint16_t        cons;
	        uint16_t        nb_rx;
		uint16_t        pad; /* reserved */
	} idx[MAX_RINGS_PER_TASK];
	struct rte_mbuf *mbuf[][MAX_RING_BURST * 3]  __rte_cache_aligned;
};

struct port_queue {
	uint8_t port;
	uint8_t queue;
} __attribute__((packed));

struct rx_params_hw {
	union {
		uint8_t           nb_rxports;
		uint8_t           rxport_mask;
	};
	uint8_t           last_read_portid;
	struct port_queue *rx_pq;
} __attribute__((packed));

struct rx_params_hw1 {
	struct port_queue rx_pq;
} __attribute__((packed));

struct rx_params_sw {
	union {
		uint8_t         nb_rxrings;
		uint8_t         rxrings_mask; /* Used if rte_is_power_of_2(nb_rxrings)*/
	};
	uint8_t         last_read_ring;
	struct rte_ring **rx_rings;
} __attribute__((packed));

/* If there is only one input ring, the pointer to it can be stored
   directly into the task_base instead of having to use a pointer to a
   set of rings which would require two dereferences. */
struct rx_params_sw1 {
	struct rte_ring *rx_ring;
} __attribute__((packed));

struct tx_params_hw {
	uint16_t          nb_txports;
	struct port_queue *tx_port_queue;
} __attribute__((packed));

struct tx_params_sw {
	uint16_t         nb_txrings;
	struct rte_ring **tx_rings;
} __attribute__((packed));

struct tx_params_hw_sw {	/* Only one port supported in this mode */
	struct port_queue tx_port_queue;
	uint16_t         nb_txrings;
	struct rte_ring **tx_rings;
} __attribute__((packed));

struct task_rt_dump {
	uint32_t n_print_rx;
	uint32_t n_print_tx;
	struct input *input;
	uint32_t n_trace;
	uint32_t cur_trace;
	void     *pkt_mbuf_addr[MAX_RING_BURST]; /* To track reordering */
	uint8_t  pkt_cpy[MAX_RING_BURST][128];
	uint16_t pkt_cpy_len[MAX_RING_BURST];
};

struct task_base;

struct task_base_aux {
	/* Not used when PROX_STATS is not defined */
	struct task_rt_stats stats;
	struct task_rt_dump task_rt_dump;

	/* Used if TASK_TSC_RX is enabled*/
	struct {
		uint16_t (*rx_pkt_orig)(struct task_base *tbase, struct rte_mbuf ***mbufs);
		uint64_t before;
		uint64_t after;
		uint64_t begin;
	} tsc_rx;

	uint32_t rx_bucket[MAX_RING_BURST + 1];
	uint16_t (*rx_pkt_orig)(struct task_base *tbase, struct rte_mbuf ***mbufs);
	uint32_t tx_bucket[MAX_RING_BURST + 1];
	void (*tx_pkt_orig)(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
	void (*tx_pkt_hw)(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
	void (*stop)(struct task_base *tbase);
	void (*start)(struct task_base *tbase);
	void (*stop_last)(struct task_base *tbase);
	void (*start_first)(struct task_base *tbase);
};

/* The task_base is accessed for _all_ task types. In case
   no debugging is needed, it has been optimized to fit
   into a single cache line to minimize cache pollution */
struct task_base {
	void (*handle_bulk)(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts);
	void (*tx_pkt)(struct task_base *tbase, struct rte_mbuf **mbufs, const uint16_t n_pkts, uint8_t *out);
	uint16_t (*rx_pkt)(struct task_base *tbase, struct rte_mbuf ***mbufs);

	struct task_base_aux* aux;
	/* The working set of mbufs contains mbufs that are currently
	   being handled. */
	struct ws_mbuf *ws_mbuf;

	uint16_t flags;

	union {
		struct rx_params_hw rx_params_hw;
		struct rx_params_hw1 rx_params_hw1;
		struct rx_params_sw rx_params_sw;
		struct rx_params_sw1 rx_params_sw1;
	};

	union {
		struct tx_params_hw tx_params_hw;
		struct tx_params_sw tx_params_sw;
		struct tx_params_hw_sw tx_params_hw_sw;
	};
} __attribute__((packed)) __rte_cache_aligned;


#endif /* _TASK_BASE_H_ */
