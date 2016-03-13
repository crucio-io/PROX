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
#include "stats_latency.h"
#include "handle_lat.h"
#include "prox_cfg.h"
#include "prox_args.h"

struct lat_stats {
	struct lat_test        lat_test;
	uint64_t               tot_max_lat;
	uint64_t               tot_min_lat;
};

struct stats_latency_manager {
	uint16_t n_latency;
	uint64_t tsc_hz;
	struct task_lat_stats *task_lats;
	struct lat_stats        lat_stats[0]; /* copy of stats when running update stats. */
};

static struct stats_latency_manager *slm;

void lat_stats_reset(void)
{
	for (uint16_t i = 0; i < slm->n_latency; ++i) {
		slm->lat_stats[i].tot_max_lat = 0;
		slm->lat_stats[i].tot_min_lat = -1;
        }
}


int stats_get_n_latency(void)
{
	return slm->n_latency;
}

struct lat_test *stats_get_lat_stats(uint32_t i)
{
	return &slm->lat_stats[i].lat_test;
}

struct task_lat_stats *stats_get_task_lats(uint32_t i)
{
	return &slm->task_lats[i];
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
	mem_size = sizeof(struct stats_latency_manager) + sizeof(struct lat_stats) * n_latency;

	ret = prox_zmalloc(mem_size, socket_id);
	ret->task_lats = prox_zmalloc(sizeof(*ret->task_lats) * n_latency, socket_id);
	return ret;
}

void stats_latency_init(void)
{
	struct lcore_cfg *lconf;
	uint32_t lcore_id = -1;

	slm = alloc_stats_latency_manager();
	slm->tsc_hz = rte_get_tsc_hz();
	while (prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			struct task_args *targ = &lconf->targs[task_id];
			if (task_runs_observable_latency(targ)) {
				if (targ->nb_rxports == 1) {
					sprintf(slm->task_lats[slm->n_latency].rx_name, "%d", targ->rx_ports[0]);
				} else if (targ->nb_rxrings == 1) {
					if (targ->rx_rings[0])
						strncpy(slm->task_lats[slm->n_latency].rx_name, targ->rx_rings[0]->name, 2);
				}
				slm->task_lats[slm->n_latency].task = (struct task_lat *)lconf->tasks_all[task_id];
				slm->task_lats[slm->n_latency].lcore_id = lcore_id;
				slm->task_lats[slm->n_latency].task_id = task_id;
				slm->lat_stats[slm->n_latency].tot_min_lat = -1;
				slm->n_latency++;
			}
		}
	}
}

uint64_t stats_core_task_lat_min(uint8_t lcore_id, uint8_t task_id)
{
	struct task_lat_stats *s;
	struct lat_test *lat_test;

	for (uint16_t i = 0; i < stats_get_n_latency(); ++i) {
		s = &slm->task_lats[i];

		if (s->lcore_id == lcore_id && s->task_id == task_id) {
			lat_test = &slm->lat_stats[i].lat_test;

			if ((lat_test->min_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				return (lat_test->min_lat << LATENCY_ACCURACY)*1000000/slm->tsc_hz;
			}
			else {
				return (lat_test->min_lat << LATENCY_ACCURACY)/(slm->tsc_hz/1000000);
			}
		}
	}

	return 0;
}

uint64_t stats_core_task_lat_max(uint8_t lcore_id, uint8_t task_id)
{
	struct task_lat_stats *s;
	struct lat_test *lat_test;

	for (uint16_t i = 0; i < stats_get_n_latency(); ++i) {
		s = &slm->task_lats[i];
		if (s->lcore_id == lcore_id && s->task_id == task_id) {
			lat_test = &slm->lat_stats[i].lat_test;

			if ((lat_test->max_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				return (lat_test->max_lat<<LATENCY_ACCURACY)*1000000/slm->tsc_hz;
			}
			else {
				return (lat_test->max_lat<<LATENCY_ACCURACY)/(slm->tsc_hz/1000000);
			}
		}
	}

	return 0;
}

uint64_t stats_core_task_tot_lat_min(uint8_t lcore_id, uint8_t task_id)
{
	struct task_lat_stats *s;
	struct lat_stats *lat_stat;

	for (uint16_t i = 0; i < stats_get_n_latency(); ++i) {
		s = &slm->task_lats[i];

		if (s->lcore_id == lcore_id && s->task_id == task_id) {
			lat_stat = &slm->lat_stats[i];

			if ((lat_stat->tot_min_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				return (lat_stat->tot_min_lat << LATENCY_ACCURACY)*1000000/slm->tsc_hz;
			}
			else {
				return (lat_stat->tot_min_lat << LATENCY_ACCURACY)/(slm->tsc_hz/1000000);
			}
		}
	}

	return 0;
}

uint64_t stats_core_task_tot_lat_max(uint8_t lcore_id, uint8_t task_id)
{
	struct task_lat_stats *s;
	struct lat_stats *lat_stat;

	for (uint16_t i = 0; i < stats_get_n_latency(); ++i) {
		s = &slm->task_lats[i];
		if (s->lcore_id == lcore_id && s->task_id == task_id) {
			lat_stat = &slm->lat_stats[i];

			if ((lat_stat->tot_max_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				return (lat_stat->tot_max_lat<<LATENCY_ACCURACY)*1000000/slm->tsc_hz;
			}
			else {
				return (lat_stat->tot_max_lat<<LATENCY_ACCURACY)/(slm->tsc_hz/1000000);
			}
		}
	}

	return 0;
}

uint64_t stats_core_task_lat_avg(uint8_t lcore_id, uint8_t task_id)
{
	struct task_lat_stats *s;
	struct lat_test *lat_test;

	for (uint16_t i = 0; i < slm->n_latency; ++i) {
		s = &slm->task_lats[i];
		if (s->lcore_id == lcore_id && s->task_id == task_id) {
			lat_test = &slm->lat_stats[i].lat_test;

			if (!lat_test->tot_pkts) {
				return 0;
			}
			if ((lat_test->tot_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				return (lat_test->tot_lat<<LATENCY_ACCURACY)*1000000/(lat_test->tot_pkts*slm->tsc_hz);
			}
			else {
				return (lat_test->tot_lat<<LATENCY_ACCURACY)/(lat_test->tot_pkts*slm->tsc_hz/1000000);
			}
		}
	}
	return 0;
}

#ifndef NO_LATENCY_PER_PACKET
void stats_core_lat(uint8_t lcore_id, uint8_t task_id, unsigned *n_pkts, uint64_t *lat)
{
	*n_pkts = 0;
	int first_packet = 0;
	for (uint16_t i = 0; i < stats_get_n_latency(); ++i) {
		struct task_lat_stats* s = &slm->task_lats[i];

		if (s->lcore_id == lcore_id && s->task_id == task_id) {
			struct lat_test *lat_test = &slm->lat_stats[i].lat_test;

			if (lat_test->tot_pkts < MAX_PACKETS_FOR_LATENCY) {
				*n_pkts = lat_test->tot_pkts ;
			} else {
				*n_pkts = MAX_PACKETS_FOR_LATENCY;
			}
			first_packet = (lat_test->cur_pkt + MAX_PACKETS_FOR_LATENCY - *n_pkts) % MAX_PACKETS_FOR_LATENCY;

			for (unsigned j = 0; j < *n_pkts && first_packet + j < MAX_PACKETS_FOR_LATENCY; j++) {
				lat[j] = (lat_test->lat[first_packet + j] << LATENCY_ACCURACY) * 1000000000/(slm->tsc_hz);
			}

			for (unsigned j = 0; j + MAX_PACKETS_FOR_LATENCY < first_packet + *n_pkts ; j++) {
				lat[j + MAX_PACKETS_FOR_LATENCY - first_packet] = (lat_test->lat[j] << LATENCY_ACCURACY) * 1000000000/(slm->tsc_hz);
			}
			plog_info("n_pkts = %d, first_packet = %d, cur_pkt = %d\n", *n_pkts, first_packet, lat_test->cur_pkt);
		}
	}
}
#endif

void stats_latency_update(void)
{
	for (uint16_t i = 0; i < slm->n_latency; ++i) {
		struct task_lat *task_lat = slm->task_lats[i].task;

		if (task_lat->use_lt != task_lat->using_lt)
			continue;

		struct lat_test *lat_test = &task_lat->lt[!task_lat->using_lt];
		if (lat_test->tot_pkts) {
			memcpy(&slm->lat_stats[i].lat_test, lat_test, sizeof(struct lat_test));
		}

		if (lat_test->max_lat > slm->lat_stats[i].tot_max_lat)
			slm->lat_stats[i].tot_max_lat = lat_test->max_lat;

		if (lat_test->min_lat < slm->lat_stats[i].tot_min_lat)
			slm->lat_stats[i].tot_min_lat = lat_test->min_lat;

		lat_test->min_rx_acc = 0;
		lat_test->max_rx_acc = 0;
		lat_test->tot_rx_acc = 0;
		lat_test->min_tx_acc = -1;
		lat_test->max_tx_acc = 0;
		lat_test->tot_tx_acc = 0;

		lat_test->tot_lat = 0;
		lat_test->var_lat = 0;
		lat_test->tot_pkts = 0;
#ifndef NO_LATENCY_PER_PACKET
		lat_test->cur_pkt = 0;
#endif
		lat_test->max_lat = 0;
		lat_test->min_lat = -1;
		memset(lat_test->buckets, 0, sizeof(lat_test->buckets));
		task_lat->use_lt = !task_lat->using_lt;
	}
}
