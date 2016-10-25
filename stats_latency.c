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

#include "prox_malloc.h"
#include "stats_latency.h"
#include "handle_lat.h"
#include "prox_cfg.h"
#include "prox_args.h"

struct stats_latency_manager_entry {
	struct task_lat        *task;
	uint8_t                lcore_id;
	uint8_t                task_id;
	struct lat_test        lat_test;
	struct lat_test        tot_lat_test;
	struct stats_latency   stats;
	struct stats_latency   tot;
};

struct stats_latency_manager {
	uint16_t n_latency;
	struct stats_latency_manager_entry entries[0]; /* copy of stats when running update stats. */
};

static struct stats_latency_manager *slm;

void stats_latency_reset(void)
{
	for (uint16_t i = 0; i < slm->n_latency; ++i)
		lat_test_reset(&slm->entries[i].tot_lat_test);
}

int stats_get_n_latency(void)
{
	return slm->n_latency;
}

uint32_t stats_latency_get_core_id(uint32_t i)
{
	return slm->entries[i].lcore_id;
}

uint32_t stats_latency_get_task_id(uint32_t i)
{
	return slm->entries[i].task_id;
}

struct stats_latency *stats_latency_get(uint32_t i)
{
	return &slm->entries[i].stats;
}

struct stats_latency *stats_latency_tot_get(uint32_t i)
{
	return &slm->entries[i].tot;
}

static struct stats_latency_manager_entry *stats_latency_entry_find(uint8_t lcore_id, uint8_t task_id)
{
	struct stats_latency_manager_entry *entry;

	for (uint16_t i = 0; i < stats_get_n_latency(); ++i) {
		entry = &slm->entries[i];

		if (entry->lcore_id == lcore_id && entry->task_id == task_id) {
			return entry;
		}
	}
	return NULL;
}

struct stats_latency *stats_latency_tot_find(uint32_t lcore_id, uint32_t task_id)
{
	struct stats_latency_manager_entry *entry = stats_latency_entry_find(lcore_id, task_id);

	if (!entry)
		return NULL;
	else
		return &entry->tot;
}

struct stats_latency *stats_latency_find(uint32_t lcore_id, uint32_t task_id)
{
	struct stats_latency_manager_entry *entry = stats_latency_entry_find(lcore_id, task_id);

	if (!entry)
		return NULL;
	else
		return &entry->stats;
}

static int task_runs_observable_latency(struct task_args *targ)
{
	/* TODO: make this work with multiple ports and with
	   rings. Currently, only showing lat tasks which have 1 RX
	   port. */
	return !strcmp(targ->task_init->mode_str, "lat") &&
		(targ->nb_rxports == 1 || targ->nb_rxrings == 1);
}

static struct stats_latency_manager *alloc_stats_latency_manager(void)
{
	const uint32_t socket_id = rte_lcore_to_socket_id(rte_lcore_id());
	struct stats_latency_manager *ret;
	struct lcore_cfg *lconf;
	uint32_t n_latency = 0;
	uint32_t lcore_id;
	size_t mem_size;

	lcore_id = -1;
	while (prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];
			if (task_runs_observable_latency(targ))
				++n_latency;
		}
	}
	mem_size = sizeof(*ret) + sizeof(ret->entries[0]) * n_latency;

	ret = prox_zmalloc(mem_size, socket_id);
	return ret;
}

static void stats_latency_add_task(struct lcore_cfg *lconf, struct task_args *targ)
{
	struct stats_latency_manager_entry *new_entry = &slm->entries[slm->n_latency];

	new_entry->task = (struct task_lat *)targ->tbase;
	new_entry->lcore_id = lconf->id;
	new_entry->task_id = targ->id;
	slm->n_latency++;
}

void stats_latency_init(void)
{
	struct lcore_cfg *lconf = NULL;
	struct task_args *targ;

	slm = alloc_stats_latency_manager();

	while (core_targ_next(&lconf, &targ, 0) == 0) {
		if (task_runs_observable_latency(targ))
			stats_latency_add_task(lconf, targ);
	}
}

#ifdef LATENCY_HISTOGRAM
void stats_core_lat_histogram(uint8_t lcore_id, uint8_t task_id, uint64_t **buckets)
{
	struct stats_latency_manager_entry *lat_stats;
	uint64_t tsc;

	lat_stats = stats_latency_entry_find(lcore_id, task_id);

	if (lat_stats)
		*buckets = lat_stats->lat_test.buckets;
	else
		*buckets = NULL;
}
#endif

static void stats_latency_fetch_entry(struct stats_latency_manager_entry *entry)
{
	struct stats_latency *cur = &entry->stats;
	struct lat_test *lat_test_local = &entry->lat_test;
	struct lat_test *lat_test_remote = task_lat_get_latency_meassurement(entry->task);

	if (!lat_test_remote)
		return;

	if (lat_test_remote->tot_all_pkts) {
		lat_test_copy(&entry->lat_test, lat_test_remote);
		lat_test_reset(lat_test_remote);
		lat_test_combine(&entry->tot_lat_test, &entry->lat_test);
	}

	task_lat_use_other_latency_meassurement(entry->task);
}

static void stats_latency_from_lat_test(struct stats_latency *dst, struct lat_test *src)
{
	/* In case packets were received, but measurements were too
	   inaccurate */
	if (src->tot_pkts) {
		dst->max = lat_test_get_max(src);
		dst->min = lat_test_get_min(src);
		dst->avg = lat_test_get_avg(src);
		dst->stddev = lat_test_get_stddev(src);
	}
	dst->accuracy_limit = lat_test_get_accuracy_limit(src);
	dst->tot_packets = src->tot_pkts;
	dst->tot_all_packets = src->tot_all_pkts;
	dst->lost_packets = src->lost_packets;
}

static void stats_latency_update_entry(struct stats_latency_manager_entry *entry)
{
	if (!entry->lat_test.tot_all_pkts)
		return;

	stats_latency_from_lat_test(&entry->stats, &entry->lat_test);
	stats_latency_from_lat_test(&entry->tot, &entry->tot_lat_test);
}

void stats_latency_update(void)
{
	for (uint16_t i = 0; i < slm->n_latency; ++i)
		stats_latency_fetch_entry(&slm->entries[i]);
	for (uint16_t i = 0; i < slm->n_latency; ++i)
		stats_latency_update_entry(&slm->entries[i]);
}
