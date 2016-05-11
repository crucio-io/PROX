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

#include <rte_lcore.h>

#include "prox_malloc.h"
#include "stats_core.h"
#include "cqm.h"
#include "log.h"
#include "msr.h"
#include "parse_utils.h"
#include "prox_cfg.h"

struct cqm_related {
	struct cqm_features	features;
	uint8_t			supported;
};

struct stats_core_manager {
	uint64_t           cqm_data_core0;
	struct cqm_related cqm;
	int                msr_support;
	uint16_t           n_lcore_stats;
	struct lcore_stats lcore_stats_set[0];
};

static struct stats_core_manager *scm;
extern int last_stat;

int stats_get_n_lcore_stats(void)
{
	return scm->n_lcore_stats;
}

int stats_cpu_freq_enabled(void)
{
	return scm->msr_support;
}

int stats_cqm_enabled(void)
{
	return scm->cqm.supported;
}

uint32_t stats_lcore_find_stat_id(uint32_t lcore_id)
{
	for (int i = 0; i < scm->n_lcore_stats; ++i)
		if (scm->lcore_stats_set[i].lcore_id == lcore_id)
			return i;
	return 0;
}

struct lcore_stats_sample *stats_get_lcore_stats_sample(uint32_t stat_id, int l)
{
	return &scm->lcore_stats_set[stat_id].sample[l == last_stat];
}

struct lcore_stats *stats_get_lcore_stats(uint32_t stat_id)
{
	return &scm->lcore_stats_set[stat_id];
}

static struct stats_core_manager *alloc_stats_core_manager(void)
{
	const int socket_id = rte_lcore_to_socket_id(rte_lcore_id());
	uint32_t n_lcore_stats = 0;
	uint32_t lcore_id;
	size_t mem_size;

	lcore_id = -1;
	while (prox_core_next(&lcore_id, 0) == 0)
		n_lcore_stats++;
	mem_size = sizeof(struct stats_core_manager) + sizeof(struct lcore_stats) * n_lcore_stats;
	return prox_zmalloc(mem_size, socket_id);
}

void stats_lcore_init(void)
{
	struct lcore_cfg *lconf;
	uint32_t lcore_id;

	scm = alloc_stats_core_manager();

	if (is_virtualized()) {
		plog_info("Not initializing msr as running in a VM\n");
		scm->msr_support = 0;
	} else if ((scm->msr_support = !msr_init()) == 0) {
		plog_warn("Failed to open msr pseudo-file (missing msr kernel module?)\n");
	}

	scm->n_lcore_stats = 0;
	lcore_id = -1;
	while (prox_core_next(&lcore_id, 0) == 0)
		scm->lcore_stats_set[scm->n_lcore_stats++].lcore_id = lcore_id;

	if (cqm_is_supported()) {
		if (!scm->msr_support) {
			plog_warn("CPU supports CQM but msr module not loaded. Disabling CQM stats.\n");
		}
		else {
			if (0 != cqm_get_features(&scm->cqm.features)) {
				plog_warn("Failed to get CQM features\n");
				scm->cqm.supported = 0;
			}
			else {
				cqm_init_stat_core(rte_lcore_id());
				scm->cqm.supported = 1;
			}

			uint32_t last_rmid = 0;
			for (uint32_t i = 0; i < scm->n_lcore_stats; ++i) {
				scm->lcore_stats_set[i].rmid = i + 1; // 0 is used by non-monitored cores
				plog_info("RMID setup: lcore = %u, RMID set to %u\n",
					  scm->lcore_stats_set[i].lcore_id,
					  scm->lcore_stats_set[i].rmid);

			}

			for (uint8_t i = 0; i < RTE_MAX_LCORE; ++i) {
				cqm_assoc(i, 0);
			}
			for (uint32_t i = 0; i < scm->n_lcore_stats; ++i) {
				cqm_assoc(scm->lcore_stats_set[i].lcore_id,
					  scm->lcore_stats_set[i].rmid);

			}
		}
	}
}

static void stats_lcore_update_freq(void)
{
	for (uint8_t i = 0; i < scm->n_lcore_stats; ++i) {
		struct lcore_stats *ls = &scm->lcore_stats_set[i];
		struct lcore_stats_sample *lss = &ls->sample[last_stat];

		msr_read(&lss->afreq, ls->lcore_id, 0xe8);
		msr_read(&lss->mfreq, ls->lcore_id, 0xe7);
	}
}

static void stats_lcore_update_cqm(void)
{
	cqm_read_ctr(&scm->cqm_data_core0, 0);

	for (uint8_t i = 0; i < scm->n_lcore_stats; ++i) {
		struct lcore_stats *ls = &scm->lcore_stats_set[i];

		if (ls->rmid)
			cqm_read_ctr(&ls->cqm_data, ls->rmid);
	}
}

void stats_lcore_post_proc(void)
{
	/* update CQM stats (calculate fraction and bytes reported) */
	uint64_t total_monitored = scm->cqm_data_core0 *
		scm->cqm.features.upscaling_factor;

	for (uint8_t i = 0; i < scm->n_lcore_stats; ++i) {
		struct lcore_stats *ls = &scm->lcore_stats_set[i];

		if (ls->rmid) {
			ls->cqm_bytes = ls->cqm_data * scm->cqm.features.upscaling_factor;
			total_monitored += ls->cqm_bytes;
		}
	}
	for (uint8_t i = 0; i < scm->n_lcore_stats; ++i) {
		struct lcore_stats *ls = &scm->lcore_stats_set[i];

		if (ls->rmid && total_monitored)
			ls->cqm_fraction = ls->cqm_bytes * 10000 / total_monitored;
		else
			ls->cqm_fraction = 0;
	}
}

void stats_lcore_update(void)
{
	if (scm->msr_support)
		stats_lcore_update_freq();
	if (scm->cqm.supported)
		stats_lcore_update_cqm();
}
