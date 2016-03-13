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

#include "prox_malloc.h"
#include "lconf.h"
#include "rx_pkt.h"
#include "tx_pkt.h"
#include "log.h"
#include "quit.h"
#include "prox_cfg.h"

struct lcore_cfg *lcore_cfg;
/* only used at initialization time */
struct lcore_cfg  lcore_cfg_init[RTE_MAX_LCORE];

static int core_targ_next_from(struct lcore_cfg **lconf, struct task_args **targ, struct lcore_cfg *lcore_cfg)
{
	uint32_t lcore_id, task_id;

	if (*lconf && *targ) {
		lcore_id = *lconf - lcore_cfg;
		task_id = *targ - lcore_cfg[lcore_id].targs;

		if (task_id + 1 < lcore_cfg[lcore_id].n_tasks_all) {
			*targ = &lcore_cfg[lcore_id].targs[task_id + 1];
			return 0;
		} else {
			if (prox_core_next(&lcore_id, 0))
				return -1;
			*lconf = &lcore_cfg[lcore_id];
			*targ = &lcore_cfg[lcore_id].targs[0];
			return 0;
		}
	} else {
		lcore_id = -1;

		if (prox_core_next(&lcore_id, 0))
			return -1;
		*lconf = &lcore_cfg[lcore_id];
		*targ = &lcore_cfg[lcore_id].targs[0];
		return 0;
	}
}

int core_targ_next(struct lcore_cfg **lconf, struct task_args **targ)
{
	return core_targ_next_from(lconf, targ, lcore_cfg);
}

int core_targ_next_early(struct lcore_cfg **lconf, struct task_args **targ)
{
	return core_targ_next_from(lconf, targ, lcore_cfg_init);
}

struct task_args *core_targ_get(uint32_t lcore_id, uint32_t task_id)
{
	return &lcore_cfg[lcore_id].targs[task_id];
}

void lcore_cfg_alloc_hp(void)
{
	size_t mem_size = RTE_MAX_LCORE * sizeof(struct lcore_cfg);

	lcore_cfg = prox_zmalloc(mem_size, rte_socket_id());
	PROX_PANIC(lcore_cfg == NULL, "Could not allocate memory for core control structures\n");
	rte_memcpy(lcore_cfg, lcore_cfg_init, mem_size);
}

int lconf_run(__attribute__((unused)) void *dummy)
{
	uint32_t lcore_id = rte_lcore_id();
	struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

	plog_info("Entering main loop on core %u\n", lcore_id);
	return lconf->thread_x(lconf);
}

static void msg_stop(struct lcore_cfg *lconf)
{
	int idx = -1;
	struct task_base *t = NULL;

	if (lconf->msg.task_id == -1) {
		for (int i = 0; i < lconf->n_tasks_all; ++i) {
			if (lconf->task_is_running[i]) {
				lconf->task_is_running[i] = 0;
				t = lconf->tasks_all[i];
				if (t->aux->stop)
				    t->aux->stop(t);
			}
		}
		lconf->n_tasks_run = 0;

		if (t && t->aux->stop_last)
			t->aux->stop_last(t);
	}
	else {
		for (int i = 0; i < lconf->n_tasks_run; ++i) {
			if (lconf_get_task_id(lconf, lconf->tasks_run[i]) == lconf->msg.task_id) {
				idx = i;
			}
			else if (idx != -1) {
				lconf->tasks_run[idx] = lconf->tasks_run[i];

				idx++;
			}
		}
		lconf->task_is_running[lconf->msg.task_id] = 0;

		t = lconf->tasks_all[lconf->msg.task_id];
		if (t->aux->stop)
			t->aux->stop(t);
		lconf->n_tasks_run--;
		if (lconf->n_tasks_run == 0 && t->aux->stop_last)
			t->aux->stop_last(t);
	}
}

static void msg_start(struct lcore_cfg *lconf)
{
	int idx = 1;
	struct task_base *t = NULL;

	if (lconf->msg.task_id == -1) {
		for (int i = 0; i < lconf->n_tasks_all; ++i) {
			t = lconf->tasks_run[i] = lconf->tasks_all[i];
			lconf->task_is_running[i] = 1;
			if (lconf->n_tasks_run == 0 && t->aux->start_first) {
				t->aux->start_first(t);
				lconf->n_tasks_run = 1;
			}
			if (t->aux->start)
				t->aux->start(t);
		}
		lconf->n_tasks_run = lconf->n_tasks_all;
	}
	else if (lconf->n_tasks_run == 0) {
		t = lconf->tasks_run[0] = lconf->tasks_all[lconf->msg.task_id];
		lconf->n_tasks_run = 1;
		lconf->task_is_running[lconf->msg.task_id] = 1;

		if (t->aux->start_first)
			t->aux->start_first(t);
		if (t->aux->start)
			t->aux->start(t);
	}
	else {
		for (int i = lconf->n_tasks_run - 1; i >= 0; --i) {
			idx = lconf_get_task_id(lconf, lconf->tasks_run[i]);
			if (idx == lconf->msg.task_id) {
				break;
			}
			else if (idx > lconf->msg.task_id) {
				lconf->tasks_run[i + 1] = lconf->tasks_run[i];
				if (i == 0) {
					lconf->tasks_run[i] = lconf->tasks_all[lconf->msg.task_id];
					lconf->n_tasks_run++;
					break;
				}
			}
			else {
				lconf->tasks_run[i + 1] = lconf->tasks_all[lconf->msg.task_id];
				lconf->n_tasks_run++;
				break;
			}
		}
		lconf->task_is_running[lconf->msg.task_id] = 1;

		if (lconf->tasks_all[lconf->msg.task_id]->aux->start)
			lconf->tasks_all[lconf->msg.task_id]->aux->start(lconf->tasks_all[lconf->msg.task_id]);
	}
}

int lconf_do_flags(struct lcore_cfg *lconf)
{
	struct task_base *t;
	int ret = 0;

	switch (lconf->msg.type) {
	case LCONF_MSG_STOP:
		msg_stop(lconf);
		ret = -1;
		break;
	case LCONF_MSG_START:
		msg_start(lconf);
		ret = -1;
		break;
	case LCONF_MSG_DUMP_RX:
	case LCONF_MSG_DUMP_TX:
	case LCONF_MSG_DUMP:
		t = lconf->tasks_all[lconf->msg.task_id];

		if (lconf->msg.val) {
			if (lconf->msg.type == LCONF_MSG_DUMP ||
			    lconf->msg.type == LCONF_MSG_DUMP_RX) {
				t->aux->task_rt_dump.n_print_rx = lconf->msg.val;
				if (t->aux->rx_pkt_orig)
					t->rx_pkt = t->aux->rx_pkt_orig;
				if (t->rx_pkt != rx_pkt_dummy) {
					t->aux->rx_pkt_orig = t->rx_pkt;
					t->rx_pkt = rx_pkt_dump;
				}
			}

			if (lconf->msg.type == LCONF_MSG_DUMP ||
			    lconf->msg.type == LCONF_MSG_DUMP_TX) {
				t->aux->task_rt_dump.n_print_tx = lconf->msg.val;
				if (t->aux->tx_pkt_orig)
					t->tx_pkt = t->aux->tx_pkt_orig;
				t->aux->tx_pkt_orig = t->tx_pkt;
				t->tx_pkt = tx_pkt_dump;
			}
		}
		break;
	case LCONF_MSG_TRACE:
		t = lconf->tasks_all[lconf->msg.task_id];

		if (lconf->msg.val) {
			t->aux->task_rt_dump.n_trace = lconf->msg.val;
			if (t->aux->rx_pkt_orig)
				t->rx_pkt = t->aux->rx_pkt_orig;

			if (t->rx_pkt != rx_pkt_dummy) {
				t->aux->rx_pkt_orig = t->rx_pkt;
				t->rx_pkt = rx_pkt_trace;
				t->aux->tx_pkt_orig = t->tx_pkt;
				t->tx_pkt = tx_pkt_trace;
			}
		}
		break;
	case LCONF_MSG_RX_DISTR_START:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];

			t->aux->rx_pkt_orig = t->rx_pkt;
			t->rx_pkt = rx_pkt_distr;
			memset(t->aux->rx_bucket, 0, sizeof(t->aux->rx_bucket));
			lconf->flags |= LCONF_FLAG_RX_DISTR_ACTIVE;
		}
		break;
	case LCONF_MSG_TX_DISTR_START:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];

			t->aux->tx_pkt_orig = t->tx_pkt;
			t->tx_pkt = tx_pkt_distr;
			memset(t->aux->tx_bucket, 0, sizeof(t->aux->tx_bucket));
			lconf->flags |= LCONF_FLAG_TX_DISTR_ACTIVE;
		}
		break;
	case LCONF_MSG_RX_DISTR_STOP:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];
			if (t->aux->rx_pkt_orig) {
				t->rx_pkt = t->aux->rx_pkt_orig;
				t->aux->rx_pkt_orig = NULL;
				lconf->flags &= ~LCONF_FLAG_RX_DISTR_ACTIVE;
			}
		}
		break;
	case LCONF_MSG_TX_DISTR_STOP:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];
			if (t->aux->tx_pkt_orig) {
				t->tx_pkt = t->aux->tx_pkt_orig;
				t->aux->tx_pkt_orig = NULL;
				lconf->flags &= ~LCONF_FLAG_TX_DISTR_ACTIVE;
			}
		}
		break;
	case LCONF_MSG_RX_DISTR_RESET:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];

			memset(t->aux->rx_bucket, 0, sizeof(t->aux->rx_bucket));
		}
		break;
	case LCONF_MSG_TX_DISTR_RESET:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];

			memset(t->aux->tx_bucket, 0, sizeof(t->aux->tx_bucket));
		}
		break;
	case LCONF_MSG_RX_BW_START:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];

			t->aux->rx_pkt_orig = t->rx_pkt;
			t->rx_pkt = rx_pkt_bw;
			lconf->flags |= LCONF_FLAG_RX_BW_ACTIVE;
		}
		break;
	case LCONF_MSG_RX_BW_STOP:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];
			if (t->aux->rx_pkt_orig) {
				t->rx_pkt = t->aux->rx_pkt_orig;
				t->aux->rx_pkt_orig = NULL;
				lconf->flags &= ~LCONF_FLAG_RX_BW_ACTIVE;
			}
		}
		break;
	case LCONF_MSG_TX_BW_START:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];

			t->aux->tx_pkt_orig = t->tx_pkt;
			t->tx_pkt = tx_pkt_bw;
			lconf->flags |= LCONF_FLAG_TX_BW_ACTIVE;
		}
		break;
	case LCONF_MSG_TX_BW_STOP:
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			t = lconf->tasks_all[task_id];
			if (t->aux->tx_pkt_orig) {
				t->tx_pkt = t->aux->tx_pkt_orig;
				t->aux->tx_pkt_orig = NULL;
				lconf->flags &= ~LCONF_FLAG_TX_BW_ACTIVE;
			}
		}
		break;
	}

	lconf_unset_req(lconf);
	return ret;
}

int lconf_get_task_id(const struct lcore_cfg *lconf, const struct task_base *task)
{
	for (int i = 0; i < lconf->n_tasks_all; ++i) {
		if (lconf->tasks_all[i] == task)
			return i;
	}

	return -1;
}

int lconf_task_is_running(const struct lcore_cfg *lconf, uint8_t task_id)
{
	return lconf->task_is_running[task_id];
}
