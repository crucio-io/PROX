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

#include "prox_malloc.h"
#include "prox_cfg.h"
#include "stats.h"
#include "stats_port.h"
#include "stats_mempool.h"
#include "stats_ring.h"
#include "stats_l4gen.h"
#include "stats_latency.h"
#include "stats_global.h"
#include "stats_core.h"
#include "stats_task.h"
#include "stats_prio_task.h"
#include "stats_latency.h"

/* Stores all readed values from the cores, displaying is done afterwards because
   displaying introduces overhead. If displaying was done right after the values
   are read, inaccuracy is introduced for later cores */
int last_stat; /* 0 or 1 to track latest 2 measurements */

void stats_reset(void)
{
	stats_task_reset();
	stats_prio_task_reset();
	stats_port_reset();
	stats_latency_reset();
	stats_global_reset();
}

void stats_init(unsigned avg_start, unsigned duration)
{
	stats_lcore_init();
	stats_task_init();
	stats_prio_task_init();
	stats_port_init();
	stats_mempool_init();
	stats_latency_init();
	stats_l4gen_init();
	stats_ring_init();
	stats_global_init(avg_start, duration);
}

void stats_update(uint16_t flag_cons)
{
	/* Keep track of last 2 measurements. */
	last_stat = !last_stat;

	if (flag_cons & STATS_CONS_F_TASKS)
		stats_task_update();

	if (flag_cons & STATS_CONS_F_PRIO_TASKS)
		stats_prio_task_update();

	if (flag_cons & STATS_CONS_F_LCORE)
		stats_lcore_update();

	if (flag_cons & STATS_CONS_F_PORTS)
		stats_port_update();

	if (flag_cons & STATS_CONS_F_MEMPOOLS)
		stats_mempool_update();

	if (flag_cons & STATS_CONS_F_LATENCY)
		stats_latency_update();

	if (flag_cons & STATS_CONS_F_L4GEN)
		stats_l4gen_update();

	if (flag_cons & STATS_CONS_F_RINGS)
		stats_ring_update();

	if (flag_cons & STATS_CONS_F_LCORE)
		stats_lcore_post_proc();

	if (flag_cons & STATS_CONS_F_TASKS)
		stats_task_post_proc();

	if (flag_cons & STATS_CONS_F_PRIO_TASKS)
		stats_prio_task_post_proc();

	if (flag_cons & STATS_CONS_F_GLOBAL)
		stats_global_post_proc();
}
