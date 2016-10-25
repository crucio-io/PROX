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

#ifndef _STATS_PRIO_TASK_H_
#define _STATS_PRIO_TASK_H_

#include <inttypes.h>

#include "clock.h"

struct prio_task_stats_sample {
	uint64_t tsc;
	uint64_t drop_tx_fail_prio[8];
	uint64_t rx_prio[8];
};

struct prio_task_rt_stats {
	uint64_t drop_tx_fail_prio[8];
	uint64_t rx_prio[8];
};

struct prio_task_stats {
	uint64_t tot_drop_tx_fail_prio[8];
	uint64_t tot_rx_prio[8];
	uint8_t lcore_id;
	uint8_t task_id;
	struct prio_task_stats_sample sample[2];
	struct prio_task_rt_stats *stats;
};

int stats_get_n_prio_tasks_tot(void);
void stats_prio_task_reset(void);
void stats_prio_task_post_proc(void);
void stats_prio_task_update(void);
void stats_prio_task_init(void);

struct prio_task_stats_sample *stats_get_prio_task_stats_sample(uint32_t task_id, int last);
struct prio_task_stats_sample *stats_get_prio_task_stats_sample_by_core_task(uint32_t lcore_id, uint32_t task_id, int last);
uint64_t stats_core_task_tot_drop_tx_fail_prio(uint8_t task_id, uint8_t prio);
uint64_t stats_core_task_tot_rx_prio(uint8_t task_id, uint8_t prio);

#endif /* _STATS_PRIO_TASK_H_ */
