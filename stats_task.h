/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2018 Viosoft Corporation.
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

#ifndef _STATS_TASK_H_
#define _STATS_TASK_H_

#include <inttypes.h>

#include "clock.h"

/* The struct task_stats is read/write from the task itself and
   read-only from the core that collects the stats. Since only the
   task executing the actual work ever modifies the stats, no locking
   is required. Both a read and a write are atomic (assuming the
   correct alignment). From this, it followed that the statistics can
   be incremented directly by the task itself. In cases where these
   assumptions do not hold, a possible solution (although slightly
   less accurate) would be to keep accumulate statistics temporarily
   in a separate structure and periodically copying the statistics to
   the statistics core through atomic primitives, for example through
   rte_atomic32_set(). The accuracy would be determined by the
   frequency at which the statistics are transferred to the statistics
   core. */

struct task_rt_stats {
	uint32_t	rx_pkt_count;
	uint32_t	tx_pkt_count;
	uint32_t	drop_tx_fail;
	uint32_t	drop_discard;
	uint32_t        drop_handled;
	uint32_t	idle_cycles;
	uint64_t        rx_bytes;
	uint64_t        tx_bytes;
	uint64_t        drop_bytes;
} __attribute__((packed)) __rte_cache_aligned;

#ifdef PROX_STATS
#define TASK_STATS_ADD_IDLE(stats, cycles) do {				\
		(stats)->idle_cycles += (cycles) + rdtsc_overhead_stats; \
	} while(0)							\

#define TASK_STATS_ADD_TX(stats, ntx) do {	\
		(stats)->tx_pkt_count += ntx;	\
	} while(0)				\

#define TASK_STATS_ADD_DROP_TX_FAIL(stats, ntx) do {	\
		(stats)->drop_tx_fail += ntx;		\
	} while(0)					\

#define TASK_STATS_ADD_DROP_HANDLED(stats, ntx) do {	\
		(stats)->drop_handled += ntx;		\
	} while(0)					\

#define TASK_STATS_ADD_DROP_DISCARD(stats, ntx) do {	\
		(stats)->drop_discard += ntx;		\
	} while(0)					\

#define TASK_STATS_ADD_RX(stats, ntx) do {	\
		(stats)->rx_pkt_count += ntx;	\
	} while (0)				\

#define TASK_STATS_ADD_RX_BYTES(stats, bytes) do {	\
		(stats)->rx_bytes += bytes;		\
	} while (0)					\

#define TASK_STATS_ADD_TX_BYTES(stats, bytes) do {	\
		(stats)->tx_bytes += bytes;		\
	} while (0)					\

#define TASK_STATS_ADD_DROP_BYTES(stats, bytes) do {	\
		(stats)->drop_bytes += bytes;		\
	} while (0)					\

#define START_EMPTY_MEASSURE() uint64_t cur_tsc = rte_rdtsc();
#else
#define TASK_STATS_ADD_IDLE(stats, cycles) do {} while(0)
#define TASK_STATS_ADD_TX(stats, ntx)  do {} while(0)
#define TASK_STATS_ADD_DROP_TX_FAIL(stats, ntx)  do {} while(0)
#define TASK_STATS_ADD_DROP_HANDLED(stats, ntx)  do {} while(0)
#define TASK_STATS_ADD_DROP_DISCARD(stats, ntx)  do {} while(0)
#define TASK_STATS_ADD_RX(stats, ntx)  do {} while(0)
#define TASK_STATS_ADD_RX_BYTES(stats, bytes)  do {} while(0)
#define TASK_STATS_ADD_TX_BYTES(stats, bytes)  do {} while(0)
#define TASK_STATS_ADD_DROP_BYTES(stats, bytes) do {} while(0)
#define START_EMPTY_MEASSURE()  do {} while(0)
#endif

struct task_stats_sample {
	uint64_t tsc;
	uint32_t tx_pkt_count;
	uint32_t drop_tx_fail;
	uint32_t drop_discard;
	uint32_t drop_handled;
	uint32_t rx_pkt_count;
	uint32_t empty_cycles;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint64_t drop_bytes;
};

struct task_stats {
	uint64_t tot_tx_pkt_count;
	uint64_t tot_drop_tx_fail;
	uint64_t tot_drop_discard;
	uint64_t tot_drop_handled;
	uint64_t tot_rx_pkt_count;

	struct task_stats_sample sample[2];

	struct task_rt_stats *stats;
	/* flags set if total RX/TX values need to be reported set at
	   initialization time, only need to access stats values in port */
	uint8_t flags;
};

void stats_task_reset(void);
void stats_task_post_proc(void);
void stats_task_update(void);
void stats_task_init(void);

int stats_get_n_tasks_tot(void);

struct task_stats *stats_get_task_stats(uint32_t lcore_id, uint32_t task_id);
struct task_stats_sample *stats_get_task_stats_sample(uint32_t lcore_id, uint32_t task_id, int last);
void stats_task_get_host_rx_tx_packets(uint64_t *rx, uint64_t *tx, uint64_t *tsc);

uint64_t stats_core_task_tot_rx(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_tot_tx(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_tot_drop(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_last_tsc(uint8_t lcore_id, uint8_t task_id);

#endif /* _STATS_TASK_H_ */
