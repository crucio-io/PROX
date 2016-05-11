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

#include <rte_mempool.h>
#include <inttypes.h>

#include "prox_malloc.h"
#include "prox_port_cfg.h"
#include "stats_mempool.h"

struct stats_mempool_manager {
	uint32_t n_mempools;
	struct mempool_stats mempool_stats[0];
};

static struct stats_mempool_manager *smm;

struct mempool_stats *stats_get_mempool_stats(uint32_t i)
{
	return &smm->mempool_stats[i];
}

int stats_get_n_mempools(void)
{
	return smm->n_mempools;
}

static struct stats_mempool_manager *alloc_stats_mempool_manager(void)
{
	const uint32_t socket_id = rte_lcore_to_socket_id(rte_lcore_id());
	uint32_t n_max_mempools = sizeof(prox_port_cfg[0].pool)/sizeof(prox_port_cfg[0].pool[0]);
	uint32_t n_mempools = 0;
	size_t mem_size = sizeof(struct stats_mempool_manager);

	for (uint8_t i = 0; i < PROX_MAX_PORTS; ++i) {
		if (!prox_port_cfg[i].active)
			continue;

		for (uint8_t j = 0; j < n_max_mempools; ++j) {
			if (prox_port_cfg[i].pool[j] && prox_port_cfg[i].pool_size[j]) {
				mem_size += sizeof(struct mempool_stats);
			}
		}
	}

	return prox_zmalloc(mem_size, socket_id);
}

void stats_mempool_init(void)
{
	uint32_t n_max_mempools = sizeof(prox_port_cfg[0].pool)/sizeof(prox_port_cfg[0].pool[0]);

	smm = alloc_stats_mempool_manager();
	for (uint8_t i = 0; i < PROX_MAX_PORTS; ++i) {
		if (!prox_port_cfg[i].active)
			continue;

		for (uint8_t j = 0; j < n_max_mempools; ++j) {
			if (prox_port_cfg[i].pool[j] && prox_port_cfg[i].pool_size[j]) {
				struct mempool_stats *ms = &smm->mempool_stats[smm->n_mempools];

				ms->pool = prox_port_cfg[i].pool[j];
				ms->port = i;
				ms->queue = j;
				ms->size = prox_port_cfg[i].pool_size[j];
				smm->n_mempools++;
			}
		}
	}
}

void stats_mempool_update(void)
{
	for (uint8_t mp_id = 0; mp_id < smm->n_mempools; ++mp_id) {
		/* Note: The function free_count returns the number of used entries. */
		smm->mempool_stats[mp_id].free = rte_mempool_count(smm->mempool_stats[mp_id].pool);
	}
}
