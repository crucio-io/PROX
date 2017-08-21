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

#ifndef _STATS_LATENCY_H_
#define _STATS_LATENCY_H_

#include <inttypes.h>

#include "handle_lat.h"

struct stats_latency {
	struct time_unit_err avg;
	struct time_unit_err min;
	struct time_unit_err max;
	struct time_unit_err stddev;

	struct time_unit accuracy_limit;
	uint64_t         lost_packets;
	uint64_t         tot_packets;
	uint64_t         tot_all_packets;
};

uint32_t stats_latency_get_core_id(uint32_t i);
uint32_t stats_latency_get_task_id(uint32_t i);
struct stats_latency *stats_latency_get(uint32_t i);
struct stats_latency *stats_latency_find(uint32_t lcore_id, uint32_t task_id);

struct stats_latency *stats_latency_tot_get(uint32_t i);
struct stats_latency *stats_latency_tot_find(uint32_t lcore_id, uint32_t task_id);

void stats_latency_init(void);
void stats_latency_update(void);
void stats_latency_reset(void);

int stats_get_n_latency(void);

#ifdef LATENCY_HISTOGRAM
void stats_core_lat_histogram(uint8_t lcore_id, uint8_t task_id, uint64_t **buckets);
#endif

#endif /* _STATS_LATENCY_H_ */
