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

#include "display_mempools.h"
#include "stats_mempool.h"
#include "display.h"
#include "defaults.h"

static struct display_page display_page_mempools;
static struct display_column *nb_col;
static struct display_column *queue_col;
static struct display_column *occup_col;
static struct display_column *used_col;
static struct display_column *free_col;
static struct display_column *total_col;
static struct display_column *mem_used_col;
static struct display_column *mem_free_col;
static struct display_column *mem_tot_col;

static void display_mempools_draw_frame(struct screen_state *screen_state)
{
	const uint32_t n_mempools = stats_get_n_mempools();

	display_page_init(&display_page_mempools);

	struct display_table *port = display_page_add_table(&display_page_mempools);
	struct display_table *stats = display_page_add_table(&display_page_mempools);

	display_table_init(port, "Port");
	display_table_init(stats, "Sampled statistics");

	nb_col = display_table_add_col(port);
	queue_col = display_table_add_col(port);
	display_column_init(nb_col, "Nb", 4);
	display_column_init(queue_col, "Queue", 5);

	occup_col = display_table_add_col(stats);
	display_column_init(occup_col, "Occup (%)", 9);
	used_col = display_table_add_col(stats);
	display_column_init(used_col, "Used (#)", 12);
	free_col = display_table_add_col(stats);
	display_column_init(free_col, "Free (#)", 12);
	total_col = display_table_add_col(stats);
	display_column_init(total_col, "Total (#)", 13);

	mem_used_col = display_table_add_col(stats);
	display_column_init(mem_used_col, "Mem Used (KB)", 13);
	mem_free_col = display_table_add_col(stats);
	display_column_init(mem_free_col, "Mem Free (KB)", 13);
	mem_tot_col = display_table_add_col(stats);
	display_column_init(mem_tot_col, "Mem Tot (KB)", 12);

	display_page_draw_frame(&display_page_mempools, n_mempools);

	for (uint16_t i = 0; i < n_mempools; ++i) {
		struct mempool_stats *ms = stats_get_mempool_stats(i);

		display_column_print(nb_col, i, "%4u", ms->port);
		display_column_print(queue_col, i, "%5u", ms->queue);
		display_column_print(total_col, i, "%13zu", ms->size);
		display_column_print(mem_tot_col, i, "%12zu", ms->size * MBUF_SIZE/1024);
	}
}

static void display_mempools_draw_stats(struct screen_state *state)
{
	const uint32_t n_mempools = stats_get_n_mempools();

	for (uint16_t i = 0; i < n_mempools; ++i) {
		struct mempool_stats *ms = stats_get_mempool_stats(i);
		const size_t used = ms->size - ms->free;
		const uint32_t used_frac = used*10000/ms->size;

		display_column_print(occup_col, i, "%6u.%02u", used_frac/100, used_frac % 100);
		display_column_print(used_col, i, "%12zu", used);
		display_column_print(free_col, i, "%12zu", ms->free);

		display_column_print(mem_free_col, i, "%13zu", used * MBUF_SIZE/1024);
		display_column_print(mem_used_col, i, "%13zu", ms->free * MBUF_SIZE/1024);
	}
}

static int display_mempools_get_height(void)
{
	return stats_get_n_mempools();
}

static struct display_screen display_screen_mempools = {
	.draw_frame = display_mempools_draw_frame,
	.draw_stats = display_mempools_draw_stats,
	.get_height = display_mempools_get_height,
	.title = "mempools",
};

struct display_screen *display_mempools(void)
{
	return &display_screen_mempools;
}
