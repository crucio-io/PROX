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

#ifndef _STATS_LATENCY_H_
#define _STATS_LATENCY_H_

#include <inttypes.h>

struct task_lat;

struct task_lat_stats {
	struct task_lat *task;
	uint8_t lcore_id;
	uint8_t task_id;
	char rx_name[2]; /* Currently only one */
};

struct lat_test *stats_get_lat_stats(uint32_t i);
struct task_lat_stats *stats_get_task_lats(uint32_t i);
struct task_l4_stats *stats_get_l4_stats(uint32_t i);
struct l4_stats_sample *stats_get_l4_stats_sample(uint32_t i, int l);

int stats_get_n_latency(void);
void stats_latency_init(void);
void stats_latency_update(void);

uint64_t stats_core_task_lat_min(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_lat_max(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_tot_lat_min(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_tot_lat_max(uint8_t lcore_id, uint8_t task_id);
uint64_t stats_core_task_lat_avg(uint8_t lcore_id, uint8_t task_id);
void lat_stats_reset(void);

#ifdef LATENCY_PER_PACKET
void stats_core_lat(uint8_t lcore_id, uint8_t task_id, unsigned int *npackets, uint64_t *lat);
#endif
#ifdef LATENCY_DETAILS
void stats_core_lat_histogram(uint8_t lcore_id, uint8_t task_id, uint64_t **buckets);
#endif

#endif /* _STATS_LATENCY_H_ */
