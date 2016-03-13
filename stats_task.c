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

#include <stddef.h>

#include "stats_task.h"
#include "prox_cfg.h"
#include "prox_globals.h"
#include "lconf.h"

struct lcore_task_stats {
	struct task_stats task_stats[MAX_TASKS_PER_CORE];
};

#define TASK_STATS_RX 0x01
#define TASK_STATS_TX 0x02

extern int last_stat;
static struct lcore_task_stats  lcore_task_stats_all[RTE_MAX_LCORE];
static struct task_stats   *task_stats_set[RTE_MAX_LCORE * MAX_TASKS_PER_CORE];
static uint8_t nb_tasks_tot;
int stats_get_n_tasks_tot(void)
{
	return nb_tasks_tot;
}

struct task_stats *stats_get_task_stats(uint32_t lcore_id, uint32_t task_id)
{
	return &lcore_task_stats_all[lcore_id].task_stats[task_id];
}

struct task_stats_sample *stats_get_task_stats_sample(uint32_t lcore_id, uint32_t task_id, int l)
{
	return &lcore_task_stats_all[lcore_id].task_stats[task_id].sample[l == last_stat];
}

void stats_task_reset(void)
{
	struct task_stats *cur_task_stats;

	for (uint8_t task_id = 0; task_id < nb_tasks_tot; ++task_id) {
		cur_task_stats = task_stats_set[task_id];
		cur_task_stats->tot_rx_pkt_count = 0;
		cur_task_stats->tot_tx_pkt_count = 0;
		cur_task_stats->tot_drop_tx_fail = 0;
		cur_task_stats->tot_drop_discard = 0;
		cur_task_stats->tot_drop_handled = 0;
	}
}

uint64_t stats_core_task_tot_rx(uint8_t lcore_id, uint8_t task_id)
{
	return lcore_task_stats_all[lcore_id].task_stats[task_id].tot_rx_pkt_count;
}

uint64_t stats_core_task_tot_tx(uint8_t lcore_id, uint8_t task_id)
{
	return lcore_task_stats_all[lcore_id].task_stats[task_id].tot_tx_pkt_count;
}

uint64_t stats_core_task_tot_drop(uint8_t lcore_id, uint8_t task_id)
{
	return lcore_task_stats_all[lcore_id].task_stats[task_id].tot_drop_tx_fail +
		lcore_task_stats_all[lcore_id].task_stats[task_id].tot_drop_discard +
		lcore_task_stats_all[lcore_id].task_stats[task_id].tot_drop_handled;
}

uint64_t stats_core_task_last_tsc(uint8_t lcore_id, uint8_t task_id)
{
	return lcore_task_stats_all[lcore_id].task_stats[task_id].sample[last_stat].tsc;
}

static void init_core_port(struct task_stats *ts, struct task_rt_stats *stats, uint8_t flags)
{
	ts->stats = stats;
	ts->flags |= flags;
}

void stats_task_post_proc(void)
{
	for (uint8_t task_id = 0; task_id < nb_tasks_tot; ++task_id) {
		struct task_stats *cur_task_stats = task_stats_set[task_id];
		const struct task_stats_sample *last = &cur_task_stats->sample[last_stat];
		const struct task_stats_sample *prev = &cur_task_stats->sample[!last_stat];

		/* no total stats for empty loops */
		cur_task_stats->tot_rx_pkt_count += last->rx_pkt_count - prev->rx_pkt_count;
		cur_task_stats->tot_tx_pkt_count += last->tx_pkt_count - prev->tx_pkt_count;
		cur_task_stats->tot_drop_tx_fail += last->drop_tx_fail - prev->drop_tx_fail;
		cur_task_stats->tot_drop_discard += last->drop_discard - prev->drop_discard;
		cur_task_stats->tot_drop_handled += last->drop_handled - prev->drop_handled;
	}
}

void stats_task_update(void)
{
	uint64_t before, after;

	for (uint8_t task_id = 0; task_id < nb_tasks_tot; ++task_id) {
		struct task_stats *cur_task_stats = task_stats_set[task_id];
		struct task_rt_stats *stats = cur_task_stats->stats;
		struct task_stats_sample *last = &cur_task_stats->sample[last_stat];

		/* Read TX first and RX second, in order to prevent displaying
		   a negative packet loss. Depending on the configuration
		   (when forwarding, for example), TX might be bigger than RX. */
		before = rte_rdtsc();
		last->tx_pkt_count = stats->tx_pkt_count;
		last->drop_tx_fail = stats->drop_tx_fail;
		last->drop_discard = stats->drop_discard;
		last->drop_handled = stats->drop_handled;
		last->rx_pkt_count = stats->rx_pkt_count;
		last->empty_cycles = stats->idle_cycles;
		last->tx_bytes     = stats->tx_bytes;
		last->rx_bytes     = stats->rx_bytes;
		last->drop_bytes   = stats->drop_bytes;
		after = rte_rdtsc();

		last->tsc = (before >> 1) + (after >> 1);
	}
}

void stats_task_get_host_rx_tx_packets(uint64_t *rx, uint64_t *tx, uint64_t *tsc)
{
	const struct task_stats *t;

	*rx = 0;
	*tx = 0;

	for (uint8_t task_id = 0; task_id < nb_tasks_tot; ++task_id) {
		t = task_stats_set[task_id];

		if (t->flags & TASK_STATS_RX)
			*rx += t->sample[last_stat].rx_pkt_count;

		if (t->flags & TASK_STATS_TX)
			*tx += t->sample[last_stat].tx_pkt_count;
	}
	if (nb_tasks_tot)
		*tsc = task_stats_set[nb_tasks_tot - 1]->sample[last_stat].tsc;
}

/* Populate active_stats_set for stats reporting, the order of the
   cores is important for gathering the most accurate statistics. TX
   cores should be updated before RX cores (to prevent negative Loss
   stats). The total number of tasks are saved in nb_tasks_tot. */
void stats_task_init(void)
{
	struct lcore_cfg *lconf;
	uint32_t lcore_id;

	/* add cores that are receiving from and sending to physical ports first */
	lcore_id = -1;
	while(prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];
			struct task_rt_stats *stats = &lconf->tasks_all[task_id]->aux->stats;
			if (targ->nb_rxrings == 0 && targ->nb_txrings == 0) {
				struct task_stats *ts = &lcore_task_stats_all[lcore_id].task_stats[task_id];

				init_core_port(ts, stats, TASK_STATS_RX | TASK_STATS_TX);
				task_stats_set[nb_tasks_tot++] = ts;
			}
		}
	}

	/* add cores that are sending to physical ports second */
	lcore_id = -1;
	while(prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];
			struct task_rt_stats *stats = &lconf->tasks_all[task_id]->aux->stats;
			if (targ->nb_rxrings != 0 && targ->nb_txrings == 0) {
				struct task_stats *ts = &lcore_task_stats_all[lcore_id].task_stats[task_id];

				init_core_port(ts, stats, TASK_STATS_TX);
				task_stats_set[nb_tasks_tot++] = ts;
			}
		}
	}

	/* add cores that are receiving from physical ports third */
	lcore_id = -1;
	while(prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];
			struct task_rt_stats *stats = &lconf->tasks_all[task_id]->aux->stats;
			if (targ->nb_rxrings == 0 && targ->nb_txrings != 0) {
				struct task_stats *ts = &lcore_task_stats_all[lcore_id].task_stats[task_id];

				init_core_port(ts, stats, TASK_STATS_RX);
				task_stats_set[nb_tasks_tot++] = ts;
			}
		}
	}

	/* add cores that are working internally (no physical ports attached) */
	lcore_id = -1;
	while(prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];
			struct task_rt_stats *stats = &lconf->tasks_all[task_id]->aux->stats;
			if (targ->nb_rxrings != 0 && targ->nb_txrings != 0) {
				struct task_stats *ts = &lcore_task_stats_all[lcore_id].task_stats[task_id];

				init_core_port(ts, stats, 0);
				task_stats_set[nb_tasks_tot++] = ts;
			}
		}
	}
}
