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

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include "display_latency.h"
#include "stats_cons.h"
#include "clock.h"

struct display_column {
	char title[32];
	int  offset;
	int  width;
	struct display_page *display_page;
};

struct display_table {
	struct display_column cols[16];
	char title[32];
	int n_cols;
	int offset;
	int width;
};

struct display_page {
	struct display_table tables[8];
	int n_tables;
	int width;
};

struct screen_state {
	unsigned chosen_screen;
	unsigned chosen_page;
	int toggle;
	int pps_unit;
};

struct display_screen {
	void (*draw_frame)(struct screen_state *screen_state);
	void (*draw_stats)(struct screen_state *screen_state);
	int (*get_height)(void);
	const char *title;
};

void display_set_pps_unit(int val);

struct lcore_cfg;
struct task_args;

void display_page_draw_frame(const struct display_page *display_page, int height);
int display_column_get_width(const struct display_column *display_column);
void display_column_init(struct display_column *display_column, const char *title, unsigned width);
struct display_column *display_table_add_col(struct display_table *table);
void display_table_init(struct display_table *table, const char *title);
struct display_table *display_page_add_table(struct display_page *display_page);
void display_page_init(struct display_page *display_page);
__attribute__((format(printf, 3, 4))) void display_column_print(const struct display_column *display_column, int row, const char *fmt, ...);
void display_column_print_core_task(const struct display_column *display_column, int row, struct lcore_cfg *lconf, struct task_args *targ);
void display_column_print_number(const struct display_column *display_column, int row, uint64_t number);

char *print_time_unit_err_usec(char *dst, struct time_unit_err *t);
char *print_time_unit_usec(char *dst, struct time_unit *t);
struct port_queue;
struct rte_ring;
void display_column_port_ring(const struct display_column *display_column, int row, struct port_queue *ports, int port_count, struct rte_ring **rings, int ring_count);

void display_init(void);
void display_end(void);
void display_stats(void);
void display_refresh(void);
void display_print(const char *str);
void display_cmd(const char *cmd, int cmd_len, int cursor_pos);
void display_screen(unsigned screen_id);
void toggle_display_screen(void);
void display_page_up(void);
void display_page_down(void);
void display_print_page(void);
void display_lock(void);
void display_unlock(void);

int display_getch(void);

static struct stats_cons display = {
	.init    = display_init,
	.notify  = display_stats,
	.refresh = display_refresh,
	.finish  = display_end,
	.flags   = STATS_CONS_F_ALL,
};

#endif /* _DISPLAY_H_ */
