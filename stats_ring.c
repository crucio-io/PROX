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

#include <inttypes.h>
#include <rte_ring.h>

#include "prox_malloc.h"
#include "stats_ring.h"
#include "prox_port_cfg.h"
#include "prox_cfg.h"
#include "lconf.h"
#include "log.h"
#include "quit.h"

struct stats_ring_manager {
	uint16_t n_rings;
	struct ring_stats ring_stats[0];
};

static struct stats_ring_manager *rsm;

int stats_get_n_rings(void)
{
	return rsm->n_rings;
}

struct ring_stats *stats_get_ring_stats(uint32_t i)
{
	return &rsm->ring_stats[i];
}

void stats_ring_update(void)
{
	for (uint16_t r_id = 0; r_id < rsm->n_rings; ++r_id) {
		rsm->ring_stats[r_id].free = rte_ring_free_count(rsm->ring_stats[r_id].ring);
	}
}

static struct ring_stats *init_rings_add(struct stats_ring_manager *rsm, struct rte_ring *ring)
{
	for (uint16_t i = 0; i < rsm->n_rings; ++i) {
		if (strcmp(ring->name, rsm->ring_stats[i].ring->name) == 0)
			return &rsm->ring_stats[i];
	}
	rsm->ring_stats[rsm->n_rings++].ring = ring;
	return &rsm->ring_stats[rsm->n_rings - 1];
}

static struct stats_ring_manager *alloc_stats_ring_manager(void)
{
	const uint32_t socket_id = rte_lcore_to_socket_id(rte_lcore_id());
	struct lcore_cfg *lconf;
	uint32_t lcore_id = -1;
	uint32_t n_rings = 0;
	struct task_args *targ;

	/* n_rings could be more than total number of rings since
	   rings could be referenced by multiple cores. */
	while(prox_core_next(&lcore_id, 1) == 0) {
		lconf = &lcore_cfg[lcore_id];

		for(uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			targ = &lconf->targs[task_id];

			for(uint32_t rxring_id = 0; rxring_id < targ->nb_rxrings; ++rxring_id) {
				if (!targ->tx_opt_ring_task)
					n_rings++;
			}
			for (uint32_t txring_id = 0; txring_id < targ->nb_txrings; ++txring_id) {
				if (!targ->tx_opt_ring)
					n_rings++;
			}
		}
	}

	for (uint8_t port_id = 0; port_id < PROX_MAX_PORTS; ++port_id) {
		if (!prox_port_cfg[port_id].active) {
			continue;
		}

		if (prox_port_cfg[port_id].rx_ring[0] != '\0')
			n_rings++;

		if (prox_port_cfg[port_id].tx_ring[0] != '\0')
			n_rings++;
	}

	size_t mem_size = sizeof(struct stats_ring_manager) +
		n_rings * sizeof(struct ring_stats);

	return prox_zmalloc(mem_size, socket_id);
}

void stats_ring_init(void)
{
	uint32_t lcore_id = -1;
	struct lcore_cfg *lconf;
	struct task_args *targ;

	rsm = alloc_stats_ring_manager();
	while(prox_core_next(&lcore_id, 1) == 0) {
		lconf = &lcore_cfg[lcore_id];

		for(uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			targ = &lconf->targs[task_id];

			for(uint32_t rxring_id = 0; rxring_id < targ->nb_rxrings; ++rxring_id) {
				if (!targ->tx_opt_ring_task)
					init_rings_add(rsm, targ->rx_rings[rxring_id]);
			}

			for (uint32_t txring_id = 0; txring_id < targ->nb_txrings; ++txring_id) {
				if (!targ->tx_opt_ring)
					init_rings_add(rsm, targ->tx_rings[txring_id]);
			}
		}
	}

	struct ring_stats *stats = NULL;

	for (uint8_t port_id = 0; port_id < PROX_MAX_PORTS; ++port_id) {
		if (!prox_port_cfg[port_id].active) {
			continue;
		}

		if (prox_port_cfg[port_id].rx_ring[0] != '\0') {
			stats = init_rings_add(rsm, rte_ring_lookup(prox_port_cfg[port_id].rx_ring));
			stats->port[stats->nb_ports++] = &prox_port_cfg[port_id];
		}

		if (prox_port_cfg[port_id].tx_ring[0] != '\0') {
			stats = init_rings_add(rsm, rte_ring_lookup(prox_port_cfg[port_id].tx_ring));
			stats->port[stats->nb_ports++] = &prox_port_cfg[port_id];
		}
	}

	/* The actual usable space for a ring is size - 1. There is at
	   most one free entry in the ring to distinguish between
	   full/empty. */
	for (uint16_t ring_id = 0; ring_id < rsm->n_rings; ++ring_id)
		rsm->ring_stats[ring_id].size = rsm->ring_stats[ring_id].ring->prod.size - 1;
}
