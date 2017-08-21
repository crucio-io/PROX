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

#include <rte_cycles.h>
#include <rte_table_hash.h>

#include "log.h"
#include "thread_generic.h"
#include "stats.h"
#include "tx_pkt.h"
#include "lconf.h"
#include "hash_entry_types.h"
#include "defines.h"
#include "hash_utils.h"

struct tsc_task {
	uint64_t tsc;
	uint64_t (* tsc_task)(struct lcore_cfg *lconf);
};

static uint64_t tsc_drain(struct lcore_cfg *lconf)
{
	lconf_flush_all_queues(lconf);
	return DRAIN_TIMEOUT;
}

static uint64_t tsc_term(struct lcore_cfg *lconf)
{
	if (lconf_is_req(lconf) && lconf_do_flags(lconf)) {
		lconf_flush_all_queues(lconf);
		return -2;
	}
	return TERM_TIMEOUT;
}

static uint64_t tsc_period(struct lcore_cfg *lconf)
{
	lconf->period_func(lconf->period_data);
	return lconf->period_timeout;
}

static uint64_t tsc_ctrl(struct lcore_cfg *lconf)
{
	const uint8_t n_tasks_all = lconf->n_tasks_all;
	void *msgs[MAX_RING_BURST];
	uint16_t n_msgs;

	for (uint8_t task_id = 0; task_id < n_tasks_all; ++task_id) {
		if (lconf->ctrl_rings_m[task_id] && lconf->ctrl_func_m[task_id]) {
#if RTE_VERSION < RTE_VERSION_NUM(17,5,0,1)
			n_msgs = rte_ring_sc_dequeue_burst(lconf->ctrl_rings_m[task_id], msgs, MAX_RING_BURST);
#else
			n_msgs = rte_ring_sc_dequeue_burst(lconf->ctrl_rings_m[task_id], msgs, MAX_RING_BURST, NULL);
#endif
			if (n_msgs) {
				lconf->ctrl_func_m[task_id](lconf->tasks_all[task_id], msgs, n_msgs);
			}
		}
		if (lconf->ctrl_rings_p[task_id] && lconf->ctrl_func_p[task_id]) {
#if RTE_VERSION < RTE_VERSION_NUM(17,5,0,1)
			n_msgs = rte_ring_sc_dequeue_burst(lconf->ctrl_rings_p[task_id], msgs, MAX_RING_BURST);
#else
			n_msgs = rte_ring_sc_dequeue_burst(lconf->ctrl_rings_p[task_id], msgs, MAX_RING_BURST, NULL);
#endif
			if (n_msgs) {
				lconf->ctrl_func_p[task_id](lconf->tasks_all[task_id], (struct rte_mbuf **)msgs, n_msgs);
			}
		}
	}
	return lconf->ctrl_timeout;
}

int thread_generic(struct lcore_cfg *lconf)
{
	struct task_base *tasks[MAX_TASKS_PER_CORE];
	int next[MAX_TASKS_PER_CORE] = {0};
	struct rte_mbuf **mbufs;
	uint64_t cur_tsc = rte_rdtsc();
	uint8_t zero_rx[MAX_TASKS_PER_CORE] = {0};
	struct tsc_task tsc_tasks[] = {
		{.tsc = cur_tsc, .tsc_task = tsc_term},
		{.tsc = cur_tsc + DRAIN_TIMEOUT, .tsc_task = tsc_drain},
		{.tsc = -1},
		{.tsc = -1},
		{.tsc = -1},
	};
	uint8_t n_tasks_run = lconf->n_tasks_run;

	if (lconf->period_func) {
		tsc_tasks[2].tsc = cur_tsc + lconf->period_timeout;
		tsc_tasks[2].tsc_task = tsc_period;
	}

	for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
		if (lconf->ctrl_func_m[task_id]) {
			tsc_tasks[3].tsc = cur_tsc + lconf->ctrl_timeout;
			tsc_tasks[3].tsc_task = tsc_ctrl;
			break;
		}
		if (lconf->ctrl_func_p[task_id]) {
			tsc_tasks[3].tsc = cur_tsc + lconf->ctrl_timeout;
			tsc_tasks[3].tsc_task = tsc_ctrl;
			break;
		}
	}

	/* sort tsc tasks */
	for (size_t i = 0; i < sizeof(tsc_tasks)/sizeof(tsc_tasks[0]); ++i) {
		for (size_t j = i + 1; j < sizeof(tsc_tasks)/sizeof(tsc_tasks[0]); ++j) {
			if (tsc_tasks[i].tsc > tsc_tasks[j].tsc) {
				struct tsc_task tmp = tsc_tasks[i];
				tsc_tasks[i] = tsc_tasks[j];
				tsc_tasks[j] = tmp;
			}
		}
	}
	struct tsc_task next_tsc = tsc_tasks[0];

	for (;;) {
		cur_tsc = rte_rdtsc();
		/* Sort scheduled tsc_tasks starting from earliest
		   first. A linear search is performed moving
		   tsc_tasks that are scheduled earlier to the front
		   of the list. There is a high frequency tsc_task in
		   most cases. As a consequence, the currently
		   scheduled tsc_task will be rescheduled to be
		   executed as the first again. If many tsc_tasks are
		   to be used, the algorithm should be replaced with a
		   priority-queue (heap). */
		if (unlikely(cur_tsc >= next_tsc.tsc)) {
			uint64_t resched_diff = tsc_tasks[0].tsc_task(lconf);

			if (resched_diff == (uint64_t)-2) {
				n_tasks_run = lconf->n_tasks_run;
				if (!n_tasks_run)
					return 0;
				for (int i = 0; i < lconf->n_tasks_run; ++i) {
					tasks[i] = lconf->tasks_run[i];

					uint8_t task_id = lconf_get_task_id(lconf, tasks[i]);
					if (lconf->targs[task_id].task_init->flag_features & TASK_FEATURE_ZERO_RX)
						zero_rx[i] = 1;
				}
			}

			uint64_t new_tsc = tsc_tasks[0].tsc + resched_diff;
			tsc_tasks[0].tsc = new_tsc;
			next_tsc.tsc = new_tsc;

			for (size_t i = 1; i < sizeof(tsc_tasks)/sizeof(tsc_tasks[0]); ++i) {
				if (new_tsc < tsc_tasks[i].tsc) {
					if (i > 1) {
						tsc_tasks[i - 1] = next_tsc;
						next_tsc = tsc_tasks[0];
					}
					break;
				}
				else
					tsc_tasks[i - 1] = tsc_tasks[i];
			}
		}

		uint16_t nb_rx;
		for (uint8_t task_id = 0; task_id < n_tasks_run; ++task_id) {
			struct task_base *t = tasks[task_id];
			struct task_args *targ = &lconf->targs[task_id];
			// Do not skip a task receiving packets from an optimized ring
			// as the transmitting task expects such a receiving task to always run and consume
			// the transmitted packets.
			if (unlikely(next[task_id] && (targ->tx_opt_ring_task == NULL))) {
				// plogx_info("task %d is too busy\n", task_id);
				next[task_id] = 0;
			} else {
				nb_rx = t->rx_pkt(t, &mbufs);
				if (likely(nb_rx || zero_rx[task_id])) {
					next[task_id] = t->handle_bulk(t, mbufs, nb_rx);
				}
			}

		}
	}
	return 0;
}
