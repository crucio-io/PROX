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

#include <rte_cycles.h>

#include "log.h"
#include "lconf.h"
#include "thread_nop.h"
#include "handle_nop.h"
#include "stats.h"
#include "lconf.h"
#include "defines.h"

int thread_nop(struct lcore_cfg *lconf)
{
	struct task_base *tasks[MAX_TASKS_PER_CORE];
	struct rte_mbuf **mbufs;
	uint64_t cur_tsc = rte_rdtsc();
	uint64_t term_tsc = cur_tsc;
	uint64_t drain_tsc = cur_tsc;
	uint8_t n_tasks_run = 0;

	for (;;) {
		cur_tsc = rte_rdtsc();
		if (cur_tsc > term_tsc) {
			term_tsc = cur_tsc + TERM_TIMEOUT;
			if (lconf_is_req(lconf) && lconf_do_flags(lconf)) {
				n_tasks_run = lconf->n_tasks_run;

				if (!n_tasks_run)
					return 0;
				for (int i = 0; i < lconf->n_tasks_run; ++i) {
					tasks[i] = lconf->tasks_run[i];
				}
			}
		}
		if (cur_tsc > drain_tsc) {
			drain_tsc = cur_tsc + DRAIN_TIMEOUT;
			lconf_flush_all_queues(lconf);
		}

		for (uint8_t task_id = 0; task_id < n_tasks_run; ++task_id) {
			struct task_base *t = tasks[task_id];
			uint16_t nb_rx = t->rx_pkt(t, &mbufs);

			if (likely(nb_rx)) {
				handle_nop_bulk(t, mbufs, nb_rx);
			}
		}
	}

	return 0;
}
