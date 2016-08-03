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

#include <string.h>

#include "prox_malloc.h"
#include "prox_cfg.h"
#include "stats_l4gen.h"
#include "task_init.h"

struct task_l4gen_stats {
	struct task_base base;
	struct l4_stats l4_stats;
};

struct stats_l4gen_manager {
	uint16_t n_l4gen;
	struct task_l4_stats task_l4_stats[0];
};

extern int last_stat;
static struct stats_l4gen_manager *sl4m;

int stats_get_n_l4gen(void)
{
	return sl4m->n_l4gen;
}

struct task_l4_stats *stats_get_l4_stats(uint32_t i)
{
	return &sl4m->task_l4_stats[i];
}

struct l4_stats_sample *stats_get_l4_stats_sample(uint32_t i, int l)
{
	return &sl4m->task_l4_stats[i].sample[l == last_stat];
}

static struct stats_l4gen_manager *alloc_stats_l4gen_manager(void)
{
	struct lcore_cfg *lconf;
	uint32_t lcore_id = -1;
	size_t mem_size;
	uint32_t n_l4gen = 0;
	const int socket_id = rte_lcore_to_socket_id(rte_lcore_id());

	lcore_id = -1;
	while (prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];

			if (!strcmp(targ->task_init->mode_str, "genl4"))
				n_l4gen++;
		}
	}

	mem_size = sizeof(struct stats_l4gen_manager) + sizeof(struct task_l4_stats) * n_l4gen;
	return prox_zmalloc(mem_size, socket_id);
}

void stats_l4gen_init(void)
{
	struct lcore_cfg *lconf;
	uint32_t lcore_id = -1;

	sl4m = alloc_stats_l4gen_manager();

	while(prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];

			if (!strcmp(targ->task_init->mode_str, "genl4")) {
				sl4m->task_l4_stats[sl4m->n_l4gen].task = (struct task_l4gen_stats *)lconf->tasks_all[task_id];
				sl4m->task_l4_stats[sl4m->n_l4gen].lcore_id = lcore_id;
				sl4m->task_l4_stats[sl4m->n_l4gen].task_id = task_id;
				sl4m->n_l4gen++;
			}
		}
	}
}

void stats_l4gen_update(void)
{
	uint64_t before, after;

	for (uint16_t i = 0; i < sl4m->n_l4gen; ++i) {
		struct task_l4gen_stats *task_l4gen = sl4m->task_l4_stats[i].task;

		before = rte_rdtsc();
		sl4m->task_l4_stats[i].sample[last_stat].stats = task_l4gen->l4_stats;
		after = rte_rdtsc();

		sl4m->task_l4_stats[i].sample[last_stat].tsc = (before >> 1) + (after >> 1);
	}
}
