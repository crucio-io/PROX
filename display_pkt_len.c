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

#include "prox_globals.h"
#include "display_pkt_len.h"
#include "stats_port.h"
#include "display.h"
#include "defaults.h"
#include "prox_port_cfg.h"
#include "clock.h"

static struct display_page display_page_pkt_len;
static struct display_column *port_col;
static struct display_column *name_col;
static struct display_column *type_col;
static struct display_column *stats_col[PKT_SIZE_COUNT];

const char *titles[] = {
	"64B (#)",
	"65-127B (#)",
	"128-255B (#)",
	"256-511B (#)",
	"512-1023B (#)",
	"1024-1522B (#)",
	"1523B+ (#)",
};

static int port_disp[PROX_MAX_PORTS];
static int n_port_disp;

static void display_pkt_len_draw_frame(struct screen_state *screen_state)
{
	n_port_disp = 0;
	for (uint8_t i = 0; i < PROX_MAX_PORTS; ++i) {
		if (prox_port_cfg[i].active) {
			port_disp[n_port_disp++] = i;
		}
	}

	display_page_init(&display_page_pkt_len);

	struct display_table *port_name = display_page_add_table(&display_page_pkt_len);

	display_table_init(port_name, "Port");
	port_col = display_table_add_col(port_name);
	name_col = display_table_add_col(port_name);
	type_col = display_table_add_col(port_name);

	display_column_init(port_col, "ID", 4);
	display_column_init(name_col, "Name", 8);
	display_column_init(type_col, "Type", 7);

	struct display_table *stats = display_page_add_table(&display_page_pkt_len);

	if (screen_state->toggle == 0)
		display_table_init(stats, "Statistics per second");
	else
		display_table_init(stats, "Total Statistics");

	for (int i = 0; i < PKT_SIZE_COUNT; ++i) {
		stats_col[i] = display_table_add_col(stats);
		display_column_init(stats_col[i], titles[i], 13);
	}

	display_page_draw_frame(&display_page_pkt_len, n_port_disp);

	for (uint8_t i = 0; i < n_port_disp; ++i) {
		const uint32_t port_id = port_disp[i];

		display_column_print(port_col, i, "%4u", port_id);
		display_column_print(name_col, i, "%8s", prox_port_cfg[port_id].name);
		display_column_print(type_col, i, "%7s", prox_port_cfg[port_id].short_name);
	}
}

static void display_pkt_len_draw_stats(struct screen_state *state)
{
	for (uint8_t i = 0; i < n_port_disp; ++i) {
		const uint32_t port_id = port_disp[i];
		struct port_stats_sample *last = stats_get_port_stats_sample(port_id, 1);
		struct port_stats_sample *prev = stats_get_port_stats_sample(port_id, 0);

		uint64_t delta_t = last->tsc - prev->tsc;
		if (delta_t == 0) // This could happen if we just reset the screen => stats will be updated later
			continue;

		if (state->toggle == 0) {
			uint64_t diff;

			for (int j = 0; j < PKT_SIZE_COUNT; ++j) {
				diff = last->tx_pkt_size[j] - prev->tx_pkt_size[j];
				display_column_print(stats_col[j], i, "%13lu", val_to_rate(diff, delta_t));
			}
		} else {
			for (int j = 0; j < PKT_SIZE_COUNT; ++j) {
				display_column_print(stats_col[j], i, "%13lu", last->tx_pkt_size[j]);
			}
		}
	}
}

static int display_pkt_len_get_height(void)
{
	return stats_get_n_ports();
}

static struct display_screen display_screen_pkt_len = {
	.draw_frame = display_pkt_len_draw_frame,
	.draw_stats = display_pkt_len_draw_stats,
	.get_height = display_pkt_len_get_height,
	.title = "pkt_len",
};

struct display_screen *display_pkt_len(void)
{
	return &display_screen_pkt_len;
}
