/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2018 Viosoft Corporation.
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
#include <rte_cycles.h>
#include <inttypes.h>

#include "stats_global.h"
#include "stats_port.h"
#include "stats_task.h"

struct global_stats {
	struct global_stats_sample sample[2];
	struct global_stats_sample beg;
	uint8_t  started_avg;
	uint64_t start_tsc;
	uint64_t end_tsc;
};

extern int last_stat;
static struct global_stats global_stats;

uint64_t stats_get_last_tsc(void)
{
	return global_stats.sample[last_stat].tsc;
}

uint64_t stats_global_start_tsc(void)
{
	return global_stats.start_tsc;
}

uint64_t stats_global_beg_tsc(void)
{
	return global_stats.beg.tsc;
}

uint64_t stats_global_end_tsc(void)
{
	return global_stats.end_tsc;
}

struct global_stats_sample *stats_get_global_stats(int last)
{
	return &global_stats.sample[last == last_stat];
}

struct global_stats_sample *stats_get_global_stats_beg(void)
{
	return (global_stats.beg.tsc < global_stats.sample[last_stat].tsc)? &global_stats.beg : NULL;
}

void stats_global_reset(void)
{
	uint64_t now = rte_rdtsc();
	uint64_t last_tsc = global_stats.sample[last_stat].tsc;
	uint64_t prev_tsc = global_stats.sample[!last_stat].tsc;
	uint64_t end_tsc = global_stats.end_tsc;

	memset(&global_stats, 0, sizeof(struct global_stats));
	global_stats.sample[last_stat].tsc = last_tsc;
	global_stats.sample[!last_stat].tsc = prev_tsc;
	global_stats.start_tsc = now;
	global_stats.beg.tsc = now;
	global_stats.end_tsc = end_tsc;
}

void stats_global_init(unsigned avg_start, unsigned duration)
{
	uint64_t now = rte_rdtsc();

	global_stats.start_tsc = now;
	/* + 1 for rounding */
	tsc_hz = rte_get_tsc_hz();
	if (duration)
		global_stats.end_tsc = global_stats.start_tsc + (avg_start + duration + 1) * tsc_hz;

	global_stats.beg.tsc = global_stats.start_tsc + avg_start * tsc_hz;
}

void stats_global_post_proc(void)
{
	uint64_t *rx = &global_stats.sample[last_stat].host_rx_packets;
	uint64_t *tx = &global_stats.sample[last_stat].host_tx_packets;
	uint64_t *tsc = &global_stats.sample[last_stat].tsc;

	stats_task_get_host_rx_tx_packets(rx, tx, tsc);
	global_stats.sample[last_stat].nics_ierrors    = stats_port_get_ierrors();
	global_stats.sample[last_stat].nics_imissed    = stats_port_get_imissed();
	global_stats.sample[last_stat].nics_rx_packets = stats_port_get_rx_packets();
	global_stats.sample[last_stat].nics_tx_packets = stats_port_get_tx_packets();

	if (global_stats.sample[last_stat].tsc > global_stats.beg.tsc && !global_stats.started_avg) {
		global_stats.started_avg = 1;
		global_stats.beg = global_stats.sample[last_stat];
	}
}
