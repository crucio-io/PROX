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

#include <rte_cycles.h>

#include "stats.h"
#include "stats_cons_log.h"
#include "prox_cfg.h"
#include "prox_args.h"
#include "commands.h"

#define STATS_DUMP_FILE_NAME "stats_dump"
static FILE *fp;

struct entry {
	uint32_t lcore_id;
	uint32_t task_id;
};

static struct entry entries[8];
static uint32_t n_entries;

struct record {
	uint32_t lcore_id;
	uint32_t task_id;
	uint64_t drop_tx_fail;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint64_t tsc;
};

void stats_cons_log_init(void)
{
	uint64_t el = rte_get_tsc_hz();
	uint64_t now = rte_rdtsc();

	fp = fopen(STATS_DUMP_FILE_NAME, "w");
	if (!fp)
		return;

	fwrite(&el, sizeof(el), 1, fp);
	fwrite(&now, sizeof(now), 1, fp);

	uint32_t lcore_id = -1;

	while(prox_core_next(&lcore_id, 0) == 0) {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];
		for (uint32_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			entries[n_entries].lcore_id = lcore_id;
			entries[n_entries].task_id = task_id;
			n_entries++;
			if (n_entries == sizeof(entries)/sizeof(entries[0]))
				return;
		}
		cmd_rx_bw_start(lcore_id);
		cmd_tx_bw_start(lcore_id);
	}
}

void stats_cons_log_notify(void)
{
	for (uint32_t i = 0; i < n_entries; ++i) {
		uint32_t c = entries[i].lcore_id;
		uint32_t t = entries[i].task_id;
		struct task_stats *l = stats_get_task_stats(c, t);
		struct task_stats_sample *last = stats_get_task_stats_sample(c, t, 1);
		static struct record buf[BUFFERED_RECORD_LEN];
		static size_t buf_pos = 0;

		buf[buf_pos].lcore_id = c;
		buf[buf_pos].task_id  = t;
		buf[buf_pos].tx_bytes = last->tx_bytes;
		buf[buf_pos].rx_bytes = last->rx_bytes;
		buf[buf_pos].drop_tx_fail = l->tot_drop_tx_fail;
		buf[buf_pos].tsc = last->tsc;

		buf_pos++;

		if (buf_pos == sizeof(buf)/sizeof(buf[0])) {
			fwrite(buf, sizeof(buf), 1, fp);
			buf_pos = 0;
		}
	}
}

void stats_cons_log_finish(void)
{
	if (fp)
		fclose(fp);
}
