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

#include "display.h"
#include "display_latency.h"
#include "stats_latency.h"
#include "lconf.h"

static struct display_column *min_col;
static struct display_column *max_col;
static struct display_column *avg_col;
static struct display_column *stddev_col;
static struct display_column *accuracy_limit_col;
static struct display_column *used_col;
static struct display_column *lost_col;
static struct display_page display_page_latency;

static void display_latency_draw_frame(struct screen_state *screen_state)
{
	const uint32_t n_latency = stats_get_n_latency();
	struct display_column *core_col;
	struct display_column *port_col;

	display_page_init(&display_page_latency);

	struct display_table *core = display_page_add_table(&display_page_latency);
	struct display_table *port = display_page_add_table(&display_page_latency);
	struct display_table *lat = display_page_add_table(&display_page_latency);
	struct display_table *acc = display_page_add_table(&display_page_latency);
	struct display_table *other = display_page_add_table(&display_page_latency);

	display_table_init(core, "Core");
	core_col = display_table_add_col(core);
	display_column_init(core_col, "Nb", 4);

	display_table_init(port, "Port Nb");
	port_col = display_table_add_col(port);
	display_column_init(port_col, "RX", 8);

	if (screen_state->toggle == 0)
		display_table_init(lat, "Measured Latency per interval");
	else
		display_table_init(lat, "Measured Latency since reset");

	min_col = display_table_add_col(lat);
	display_column_init(min_col, "Min (us)", 20);
	max_col = display_table_add_col(lat);
	display_column_init(max_col, "Max (us)", 20);
	avg_col = display_table_add_col(lat);
	display_column_init(avg_col, "Avg (us)", 20);
	stddev_col = display_table_add_col(lat);
	display_column_init(stddev_col, "Stddev (us)", 20);

	display_table_init(acc, "Accuracy ");
	used_col = display_table_add_col(acc);
	display_column_init(used_col, "Used Packets (%)", 16);
	accuracy_limit_col = display_table_add_col(acc);
	display_column_init(accuracy_limit_col, "limit (us)", 16);

	display_table_init(other, "Other");

	lost_col = display_table_add_col(other);
	display_column_init(lost_col, "Lost Packets", 16);

	display_page_draw_frame(&display_page_latency, n_latency);

	for (uint16_t i = 0; i < n_latency; ++i) {
		uint32_t lcore_id = stats_latency_get_core_id(i);
		uint32_t task_id = stats_latency_get_task_id(i);
		struct task_args *targ = &lcore_cfg[lcore_id].targs[task_id];

		display_column_print(core_col, i, "%2u/%1u", lcore_id, task_id);
		display_column_port_ring(port_col, i, targ->rx_port_queue, targ->nb_rxports, targ->rx_rings, targ->nb_rxrings);
	}
}

#define AFTER_POINT 1000000

static void display_stats_latency_entry(int row, struct stats_latency *stats_latency)
{
	struct time_unit_err avg = stats_latency->avg;
	struct time_unit_err min = stats_latency->min;
	struct time_unit_err max = stats_latency->max;
	struct time_unit_err stddev = stats_latency->stddev;
	struct time_unit accuracy_limit = stats_latency->accuracy_limit;

	uint32_t used = 0;

	if (stats_latency->tot_all_packets)
		used = stats_latency->tot_packets * (100 * AFTER_POINT) / stats_latency->tot_all_packets;

	char dst[32];

	if (stats_latency->tot_packets) {
		display_column_print(min_col, row, "%s", print_time_unit_err_usec(dst, &min));
		display_column_print(max_col, row, "%s", print_time_unit_err_usec(dst, &max));
		display_column_print(avg_col, row, "%s", print_time_unit_err_usec(dst, &avg));
		display_column_print(stddev_col, row, "%s", print_time_unit_err_usec(dst, &stddev));
	} else {
		display_column_print(min_col, row, "%s", "N/A");
		display_column_print(max_col, row, "%s", "N/A");
		display_column_print(avg_col, row, "%s", "N/A");
		display_column_print(stddev_col, row, "%s", "N/A");
	}

	display_column_print(accuracy_limit_col, row, "%s", print_time_unit_usec(dst, &accuracy_limit));
	display_column_print(lost_col, row, "%16"PRIu64"", stats_latency->lost_packets);
	display_column_print(used_col, row, "%3u.%06u", used / AFTER_POINT, used % AFTER_POINT);
}

static void display_latency_draw_stats(struct screen_state *screen_state)
{
	const uint32_t n_latency = stats_get_n_latency();
	struct stats_latency *stats_latency;

	for (uint16_t i = 0; i < n_latency; ++i) {
		if (screen_state->toggle == 0)
			stats_latency = stats_latency_get(i);
		else
			stats_latency = stats_latency_tot_get(i);

		display_stats_latency_entry(i, stats_latency);
	}
}

static int display_latency_get_height(void)
{
	return stats_get_n_latency();
}

static struct display_screen display_screen_latency = {
	.draw_frame = display_latency_draw_frame,
	.draw_stats = display_latency_draw_stats,
	.get_height = display_latency_get_height,
	.title = "latency",
};

struct display_screen *display_latency(void)
{
	return &display_screen_latency;
}
