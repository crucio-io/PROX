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

#include <curses.h>

#include <rte_cycles.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <signal.h>

#include "prox_malloc.h"
#include "handle_lat.h"
#include "cqm.h"
#include "msr.h"
#include "display.h"
#include "log.h"
#include "commands.h"
#include "main.h"
#include "stats.h"
#include "stats_port.h"
#include "stats_mempool.h"
#include "stats_ring.h"
#include "stats_l4gen.h"
#include "stats_latency.h"
#include "stats_global.h"
#include "stats_core.h"
#include "prox_cfg.h"
#include "prox_assert.h"
#include "version.h"
#include "quit.h"
#include "prox_port_cfg.h"
#include "genl4_bundle.h"

struct screen_state {
	char chosen_screen;
	int chosen_page;
};

static struct screen_state screen_state;
static int col_offset;
/* Set up the display mutex  as recursive. This enables threads to use
   display_[un]lock() to lock  the display when multiple  calls to for
   instance plog_info() need to be made. */
static pthread_mutex_t disp_mtx = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;


static void stats_display_layout(uint8_t in_place);


static void display_lock(void)
{
	pthread_mutex_lock(&disp_mtx);
}

static void display_unlock(void)
{
	pthread_mutex_unlock(&disp_mtx);
}

struct task_stats_disp {
	uint32_t lcore_id;
	uint32_t task_id;
	uint32_t lcore_stat_id;
};

static struct task_stats_disp task_stats_disp[RTE_MAX_LCORE * MAX_TASKS_PER_CORE];
static int port_disp[PROX_MAX_PORTS];
static int n_port_disp;

/* Advanced text output */
static WINDOW *scr = NULL, *win_txt, *win_general, *win_cmd, *win_stat, *win_title, *win_tabs, *win_help;
static int win_txt_height = 1;
static int title_len;

static uint16_t core_port_height;
static uint16_t max_n_lines;
static uint64_t tsc_hz;

static int cmd_cursor_pos;
static const char *cmd_cmd;
static int cmd_len;

/* Colors used in the interface */
enum colors {
	INVALID_COLOR,
	NO_COLOR,
	RED_ON_BLACK,
	BLACK_ON_CYAN,
	BLACK_ON_GREEN,
	BLACK_ON_WHITE,
	BLACK_ON_YELLOW,
	YELLOW_ON_BLACK,
	WHITE_ON_RED,
	YELLOW_ON_NOTHING,
	GREEN_ON_NOTHING,
	RED_ON_NOTHING,
	BLUE_ON_NOTHING,
	CYAN_ON_NOTHING,
	MAGENTA_ON_NOTHING,
	WHITE_ON_NOTHING,
};

static uint64_t div_round(uint64_t val, uint64_t delta_t)
{
	const uint64_t thresh = UINT64_MAX/tsc_hz;

	if (val < thresh) {
		return val * tsc_hz / delta_t;
	} else {
		if (delta_t < tsc_hz)
			return 0;
		else
			return val / (delta_t/tsc_hz);
	}

}

int display_getch(void)
{
	int ret;

	display_lock();
	ret = wgetch(scr);
	display_unlock();

	return ret;
}

void display_cmd(const char *cmd, int cl, int cursor_pos)
{
	cmd_len = cl;
	if (cursor_pos == -1 || cursor_pos > cmd_len)
		cursor_pos = cmd_len;
	cmd_cursor_pos = cursor_pos;
	cmd_cmd = cmd;

	display_lock();
	werase(win_cmd);
	if (cursor_pos < cmd_len) {
		waddnstr(win_cmd, cmd, cursor_pos);
		wbkgdset(win_cmd, COLOR_PAIR(YELLOW_ON_BLACK));
		waddnstr(win_cmd, cmd + cursor_pos, 1);
		wbkgdset(win_cmd, COLOR_PAIR(BLACK_ON_YELLOW));
		waddnstr(win_cmd, cmd + cursor_pos + 1, cmd_len - (cursor_pos + 1));
	}
	else {
		waddnstr(win_cmd, cmd, cmd_len);
		wmove(win_cmd, cursor_pos, 0);
		wbkgdset(win_cmd, COLOR_PAIR(YELLOW_ON_BLACK));
		waddstr(win_cmd, " ");
		wbkgdset(win_cmd, COLOR_PAIR(BLACK_ON_YELLOW));
	}

	wattroff(win_stat, A_UNDERLINE);
	wrefresh(win_cmd);
	display_unlock();
}

static void refresh_cmd_win(void)
{
	display_cmd(cmd_cmd, cmd_len, cmd_cursor_pos);
}

static WINDOW *create_subwindow(int height, int width, int y_pos, int x_pos)
{
	WINDOW *win = subwin(scr, height, width, y_pos, x_pos);
	touchwin(scr);
	return win;
}

/* Format string capable [mv]waddstr() wrappers */
__attribute__((format(printf, 4, 5))) static inline int mvwaddstrf(WINDOW* win, int y, int x, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	wmove(win, y, x);
	if (x > COLS - 1) {
		return 0;
	}

	/* to prevent strings from wrapping and */
	if (strlen(buf) > (uint32_t)COLS - x) {
		buf[COLS - 1 - x] = 0;
	}
	waddstr(win, buf);
	return ret;
}

// Red: link down; Green: link up
static short link_color(const uint8_t if_port)
{
	return COLOR_PAIR(prox_port_cfg[if_port].link_up? GREEN_ON_NOTHING : RED_ON_NOTHING);
}

static void (*ncurses_sigwinch)(int);

static void sigwinch(int in)
{
	if (ncurses_sigwinch)
		ncurses_sigwinch(in);
	refresh();
	stats_display_layout(0);
}

static void set_signal_handler(void)
{
	struct sigaction old;

	sigaction(SIGWINCH, NULL, &old);
	ncurses_sigwinch = old.sa_handler;

	signal(SIGWINCH, sigwinch);
}

void display_init(void)
{
	scr = initscr();
	start_color();
	/* Assign default foreground/background colors to color number -1 */
	use_default_colors();
	tsc_hz = rte_get_tsc_hz();

	init_pair(NO_COLOR,   -1,  -1);
	init_pair(RED_ON_BLACK,     COLOR_RED,  COLOR_BLACK);
	init_pair(BLACK_ON_CYAN,   COLOR_BLACK,  COLOR_CYAN);
	init_pair(BLACK_ON_GREEN,  COLOR_BLACK,  COLOR_GREEN);
	init_pair(BLACK_ON_WHITE,  COLOR_BLACK,  COLOR_WHITE);
	init_pair(BLACK_ON_YELLOW, COLOR_BLACK,  COLOR_YELLOW);
	init_pair(YELLOW_ON_BLACK, COLOR_YELLOW,  COLOR_BLACK);
	init_pair(WHITE_ON_RED,    COLOR_WHITE,  COLOR_RED);
	init_pair(YELLOW_ON_NOTHING,   COLOR_YELLOW,  -1);
	init_pair(GREEN_ON_NOTHING,   COLOR_GREEN,  -1);
	init_pair(RED_ON_NOTHING,   COLOR_RED,  -1);
	init_pair(BLUE_ON_NOTHING,  COLOR_BLUE, -1);
	init_pair(CYAN_ON_NOTHING,  COLOR_CYAN, -1);
	init_pair(MAGENTA_ON_NOTHING,  COLOR_MAGENTA, -1);
	init_pair(WHITE_ON_NOTHING,  COLOR_WHITE, -1);
	/* nodelay(scr, TRUE); */
	noecho();
	curs_set(0);
	/* Create fullscreen log window. When stats are displayed
	   later, it is recreated with appropriate dimensions. */
	win_txt = create_subwindow(0, 0, 0, 0);
	wbkgd(win_txt, COLOR_PAIR(0));

	idlok(win_txt, FALSE);
	/* Get scrolling */
	scrollok(win_txt, TRUE);
	/* Leave cursor where it was */
	leaveok(win_txt, TRUE);

	refresh();

	set_signal_handler();

	max_n_lines = (LINES - 5 - 2 - 3);
	core_port_height = max_n_lines < stats_get_n_tasks_tot()? max_n_lines : stats_get_n_tasks_tot();

	for (uint8_t i = 0; i < PROX_MAX_PORTS; ++i) {
		if (prox_port_cfg[i].active) {
			port_disp[n_port_disp++] = i;
		}
	}

	stats_display_layout(0);

	cmd_rx_tx_info();
	if (get_n_warnings() == -1) {
		plog_info("Warnings disabled\n");
	}
	else if (get_n_warnings() > 0) {
		int n_print = get_n_warnings() < 5? get_n_warnings(): 5;
		plog_info("Started with %d warnings, last %d warnings: \n", get_n_warnings(), n_print);
		for (int i = -n_print + 1; i <= 0; ++i) {
			plog_info("%s", get_warning(i));
		}
	}
	else {
		plog_info("Started without warnings\n");
	}
}

static void stats_display_latency(void)
{
	const uint32_t n_latency = stats_get_n_latency();

	display_lock();

	wattron(win_stat, A_BOLD);
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));

	/* Labels */
	mvwaddstrf(win_stat, 0, 0,   "Core");
	mvwaddstrf(win_stat, 1, 0,   "  Nb");
	mvwvline(win_stat, 0, 4,  ACS_VLINE, n_latency + 2);
	mvwaddstrf(win_stat, 0, 5, " Port Nb");
	mvwaddstrf(win_stat, 1, 5, "  RX");

	mvwaddstrf(win_stat, 0, 36, "Measured Latency");

	mvwvline(win_stat, 0, 13,  ACS_VLINE, n_latency + 2);
	mvwaddstrf(win_stat, 1, 14, "  Min (us)");
	mvwvline(win_stat, 1, 26,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 27, "  Max (us)");
	mvwvline(win_stat, 1, 39,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 40, "  Avg (us)");
	mvwvline(win_stat, 1, 52,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 53, " STDDev (us)");
	mvwvline(win_stat, 1, 65,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 68, " Lost Packets");
	mvwvline(win_stat, 0, 82,  ACS_VLINE, n_latency + 2);

	mvwaddstrf(win_stat, 0, 97, "RX Accuracy");
	mvwaddstrf(win_stat, 1, 114-30, "  Min (ns)");
	mvwvline(win_stat, 1, 126-30,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 127-30, "  Max (ns)");
	mvwvline(win_stat, 1, 139-30,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 140-30, "  Avg (ns)");
	mvwvline(win_stat, 1, 152-30,  ACS_VLINE, n_latency + 1);

	mvwaddstrf(win_stat, 0, 136, "TX Accuracy");
	mvwaddstrf(win_stat, 1, 153-30, "  Min (ns)");
	mvwvline(win_stat, 1, 165-30,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 166-30, "  Max (ns)");
	mvwvline(win_stat, 1, 178-30,  ACS_VLINE, n_latency + 1);
	mvwaddstrf(win_stat, 1, 179-30, "  Avg (ns)");
	mvwvline(win_stat, 1, 191-30,  ACS_VLINE, n_latency + 1);

	mvwvline(win_stat, 1, 82,  ACS_VLINE, n_latency + 1);
	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
	wattroff(win_stat, A_BOLD);

	for (uint16_t i = 0; i < n_latency; ++i) {
		struct task_lat_stats *tl = stats_get_task_lats(i);

		mvwaddstrf(win_stat, 2 + i, 0, "%2u/%1u", tl->lcore_id, tl->task_id);
		mvwaddstrf(win_stat, 2 + i, 8, "%s", tl->rx_name);
	}

	display_unlock();
}

static void stats_display_l4gen(void)
{
	const uint32_t n_l4gen = stats_get_n_l4gen();

	display_lock();

	wattron(win_stat, A_BOLD);
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));

	/* Labels */
	mvwaddstrf(win_stat, 0, 0,   "Core");
	mvwaddstrf(win_stat, 1, 0,   "  Nb");

	mvwaddstrf(win_stat, 0, 4, "       Setup rate (flows/s)          ");
	mvwvline(win_stat, 0, 4,  ACS_VLINE, n_l4gen + 2);
	mvwaddstrf(win_stat, 1, 5, "   TCP   ");
	mvwvline(win_stat, 1, 14,  ACS_VLINE, n_l4gen + 1);
	mvwaddstrf(win_stat, 1, 15, "   UDP   ");
	mvwvline(win_stat, 1, 24,  ACS_VLINE, n_l4gen + 1);
	mvwaddstrf(win_stat, 1, 25, "TCP + UDP");
	mvwvline(win_stat, 0, 34,  ACS_VLINE, n_l4gen + 2);
	mvwaddstrf(win_stat, 1, 35, "  Bundles  ");
	mvwvline(win_stat, 0, 34 +12,  ACS_VLINE, n_l4gen + 2);

	mvwaddstrf(win_stat, 0, 35 +12, "        Teardown rate (flows/s)      ");
	mvwaddstrf(win_stat, 1, 35 +12, "TCP w/o reTX");
	mvwvline(win_stat, 1, 47 +12,  ACS_VLINE, n_l4gen + 1);
	mvwaddstrf(win_stat, 1, 48 +12, "TCP w/  reTX");
	mvwvline(win_stat, 1, 60 +12,  ACS_VLINE, n_l4gen + 1);
	mvwaddstrf(win_stat, 1, 61 +12, "     UDP    ");
	mvwvline(win_stat, 0, 73 +12,  ACS_VLINE, n_l4gen + 2);


	mvwaddstrf(win_stat, 0, 74 +12, "Expire rate (flows/s)");
	mvwaddstrf(win_stat, 1, 74 +12, "    TCP   ");
	mvwvline(win_stat, 1, 84 +12,  ACS_VLINE, n_l4gen + 1);
	mvwaddstrf(win_stat, 1, 85 +12, "    UDP   ");
	mvwvline(win_stat, 0, 95 +12,  ACS_VLINE, n_l4gen + 2);

	mvwaddstrf(win_stat, 0, 96 +12, "         Other       ");
	mvwaddstrf(win_stat, 1, 96 +12, "active (#)");
	mvwvline(win_stat, 1, 106 +12,  ACS_VLINE, n_l4gen + 1);
	mvwaddstrf(win_stat, 1, 107 +12, " reTX (/s)");
	mvwvline(win_stat, 0, 117 +12,  ACS_VLINE, n_l4gen + 2);

	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
	wattroff(win_stat, A_BOLD);

	for (uint16_t i = 0; i < n_l4gen; ++i) {
		struct task_l4_stats *tls = stats_get_l4_stats(i);

		mvwaddstrf(win_stat, 2 + i, 0, "%2u/%1u", tls->lcore_id, tls->task_id);
	}

	display_unlock();
}

static void stats_display_mempools(void)
{
	const uint32_t n_mempools = stats_get_n_mempools();

	display_lock();

	wattron(win_stat, A_BOLD);
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));
	/* Labels */
	mvwaddstrf(win_stat, 0, 2,   "Port");
	mvwaddstrf(win_stat, 1, 0,   "  Nb");
	mvwvline(win_stat, 1, 4,  ACS_VLINE, n_mempools + 2);
	mvwaddstrf(win_stat, 1, 5,   "Queue");

	mvwvline(win_stat, 0, 10,  ACS_VLINE, n_mempools + 3);
	mvwaddstrf(win_stat, 0, 50, "Sampled statistics");
	mvwaddstrf(win_stat, 1, 11, "Occup (%%)");
	mvwvline(win_stat, 1, 20,  ACS_VLINE, n_mempools + 1);
	mvwaddstrf(win_stat, 1, 21, "    Used (#)");
	mvwvline(win_stat, 1, 33,  ACS_VLINE, n_mempools + 1);
	mvwaddstrf(win_stat, 1, 34, "    Free (#)");
	mvwvline(win_stat, 1, 46,  ACS_VLINE, n_mempools + 1);
	mvwaddstrf(win_stat, 1, 47, "   Total (#)");
	mvwvline(win_stat, 1, 59,  ACS_VLINE, n_mempools + 1);
	mvwaddstrf(win_stat, 1, 60, " Mem Used (KB)");
	mvwvline(win_stat, 1, 74,  ACS_VLINE, n_mempools + 1);
	mvwaddstrf(win_stat, 1, 75, " Mem Free (KB)");
	mvwvline(win_stat, 1, 89,  ACS_VLINE, n_mempools + 1);
	mvwaddstrf(win_stat, 1, 90, " Mem Tot  (KB)");
	mvwvline(win_stat, 0, 104,  ACS_VLINE, n_mempools + 2);
	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
	wattroff(win_stat, A_BOLD);

	for (uint16_t i = 0; i < n_mempools; ++i) {
		struct mempool_stats *ms = stats_get_mempool_stats(i);

		mvwaddstrf(win_stat, 2 + i, 0, "%4u", ms->port);
		mvwaddstrf(win_stat, 2 + i, 5, "%5u", ms->queue);
		mvwaddstrf(win_stat, 2 + i, 47, "%12zu", ms->size);
		mvwaddstrf(win_stat, 2 + i, 90, "%14zu", ms->size * MBUF_SIZE/1024);
	}

	display_unlock();
}

static void stats_display_rings(void)
{
	const uint32_t n_rings = stats_get_n_rings() > max_n_lines? max_n_lines : stats_get_n_rings();

	int top = 1;
	int left = 11;

	display_lock();

	wattron(win_stat, A_BOLD);
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));
	mvwaddstrf(win_stat, 0, 31, "Ring Information");
	/* Labels */
	mvwaddstrf(win_stat, top, left-5, "Ring");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);
	left += 12;

	mvwaddstrf(win_stat, top, left-5, "Port");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);

	left += 12;
	mvwaddstrf(win_stat, top, left-9, "Occup (%%)");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);

	left += 10;
	mvwaddstrf(win_stat, top, left-5, "Free");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);

	left += 10;
	mvwaddstrf(win_stat, top, left-5, "Size");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);

	left += 3;
	mvwaddstrf(win_stat, top, left-2, "SC");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);

	left += 3;
	mvwaddstrf(win_stat, top, left-2, "SP");
	mvwvline(win_stat, top, left, ACS_VLINE, n_rings + 1);
	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
	wattroff(win_stat, A_BOLD);

	top++;

	for (uint16_t i = 0; i < n_rings; ++i) {
		struct ring_stats *rs = stats_get_ring_stats(i);

		left = 0;
		mvwaddstrf(win_stat, top, left, "%s", rs->ring->name);
		left += 12;
		for (uint32_t j = 0; j < rs->nb_ports; j++) {
			mvwaddstrf(win_stat, top+j, left, "%s", rs->port[j]->name);
		}

		left += 12 + 12 + 10 + 10;
		mvwaddstrf(win_stat, top, left, "%s", (rs->ring->flags & RING_F_SC_DEQ) ? " y" : " n");
		left += 3;
		mvwaddstrf(win_stat, top, left, "%s", (rs->ring->flags & RING_F_SP_ENQ) ? " y" : " n" );
		top += rs->nb_ports ? rs->nb_ports : 1;
	}

	display_unlock();
}

static void stats_display_pkt_len(void)
{
	const uint32_t n_ports = stats_get_n_ports();
	char name[32];
	char *ptr;

	display_lock();
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));
	wattron(win_stat, A_BOLD);
	/* Labels */
	mvwaddstrf(win_stat, 0, 2,   "Port");
	mvwaddstrf(win_stat, 1, 0,   "  Nb");
	mvwvline(win_stat, 1, 4,  ACS_VLINE, n_ports + 2);
	mvwaddstrf(win_stat, 1, 5,   "Name");
	mvwvline(win_stat, 1, 13,  ACS_VLINE, n_ports + 2);
	mvwaddstrf(win_stat, 1, 14,   "Type");

	mvwvline(win_stat, 0, 21,  ACS_VLINE, n_ports + 3);
	mvwaddstrf(win_stat, 0, 54, "Statistics per second");

	mvwaddstrf(win_stat, 1, 22, "      64B (#)");
	mvwvline(win_stat, 1, 35,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 37, "  65-127B (#)");
	mvwvline(win_stat, 1, 36+14*1,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*1 + 1, " 128-255B (#)");
	mvwvline(win_stat, 1, 36+14*2,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*2+1, " 256-511B (#)");
	mvwvline(win_stat, 1, 36+14*3,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*3+1, "512-1023B (#)");
	mvwvline(win_stat, 1, 36+14*4,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*4+1, "   1024B+ (#)");
	mvwvline(win_stat, 0, 36+14*5,  ACS_VLINE, n_ports + 2);

	mvwaddstrf(win_stat, 0, 15+14*5+54, "Total Statistics");

	mvwaddstrf(win_stat, 1, 36+14*5+1, "      64B (#)");
	mvwvline(win_stat, 1, 36+14*6,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*6+1, "  65-127B (#)");
	mvwvline(win_stat, 1, 36+14*7,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*7+1, " 128-255B (#)");
	mvwvline(win_stat, 1, 36+14*8,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*8+1, " 256-511B (#)");
	mvwvline(win_stat, 1, 36+14*9,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*9+1, "512-1023B (#)");
	mvwvline(win_stat, 1, 36+14*10,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 36+14*10+1, "   1024B+ (#)");
	mvwvline(win_stat, 0, 36+14*11,  ACS_VLINE, n_ports + 2);

	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
	wattroff(win_stat, A_BOLD);
	uint8_t count = 0;
	for (uint8_t i = 0; i < n_port_disp; ++i) {
		const uint32_t port_id = port_disp[i];

		mvwaddstrf(win_stat, 2 + count, 0, "%4u", port_id);
		mvwaddstrf(win_stat, 2 + count, 5, "%8s", prox_port_cfg[port_id].name);
		strncpy(name, prox_port_cfg[port_id].driver_name, 31);
		if ((ptr = strstr(name, "_pmd")) != NULL) {
			*ptr = '\x0';
		}
		if (strncmp(name, "rte_", 4) == 0) {
			mvwaddstrf(win_stat, 2 + count, 14, "%7s", name+4);
		} else {
			mvwaddstrf(win_stat, 2 + count, 14, "%7s", name);
		}
		count++;
	}
	display_unlock();
}

static void stats_display_eth_ports(void)
{
	const uint32_t n_ports = stats_get_n_ports();
	char name[32];
	char *ptr;

	display_lock();
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));
	wattron(win_stat, A_BOLD);
	/* Labels */
	mvwaddstrf(win_stat, 0, 2,   "Port");
	mvwaddstrf(win_stat, 1, 0,   "  Nb");
	mvwvline(win_stat, 1, 4,  ACS_VLINE, n_ports + 2);
	mvwaddstrf(win_stat, 1, 5,   "Name");
	mvwvline(win_stat, 1, 13,  ACS_VLINE, n_ports + 2);
	mvwaddstrf(win_stat, 1, 14,   "Type");

	mvwvline(win_stat, 0, 21,  ACS_VLINE, n_ports + 3);
	mvwaddstrf(win_stat, 0, 22, "                        Statistics per second");
	mvwaddstrf(win_stat, 1, 22, "no mbufs (#)");
	mvwvline(win_stat, 1, 34,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 35, "ierrors (#)");
	mvwvline(win_stat, 1, 46,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 47, "oerrors (#)");
	mvwvline(win_stat, 1, 58,  ACS_VLINE, n_ports + 1);

	mvwaddstrf(win_stat, 1, 47+12, "RX (Kpps)");
	mvwvline(win_stat, 1, 56+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 57+12, "TX (Kpps)");
	mvwvline(win_stat, 1, 66+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 67+12, "RX (Kbps)");
	mvwvline(win_stat, 1, 76+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 77+12, "TX (Kbps)");
	mvwvline(win_stat, 1, 86+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 87+12, "  RX (%%)");
	mvwvline(win_stat, 1, 95+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 96+12, "  TX (%%)");
	mvwvline(win_stat, 0, 104+12,  ACS_VLINE, n_ports + 2);

	mvwaddstrf(win_stat, 0, 105+12, "                        Total Statistics");
	mvwaddstrf(win_stat, 1, 105+12, "           RX");
	mvwvline(win_stat, 1, 118+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 119+12, "           TX");
	mvwvline(win_stat, 1, 132+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 133+12, " no mbufs (#)");
	mvwvline(win_stat, 1, 146+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 147+12, "  ierrors (#)");
	mvwvline(win_stat, 1, 160+12,  ACS_VLINE, n_ports + 1);
	mvwaddstrf(win_stat, 1, 161+12, "  oerrors (#)");
	mvwvline(win_stat, 0, 174+12,  ACS_VLINE, n_ports + 2);
	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
	wattroff(win_stat, A_BOLD);
	uint8_t count = 0;
	for (uint8_t i = 0; i < n_port_disp; ++i) {
		const uint32_t port_id = port_disp[i];

		mvwaddstrf(win_stat, 2 + count, 0, "%4u", port_id);
		mvwaddstrf(win_stat, 2 + count, 5, "%8s", prox_port_cfg[port_id].name);
		strncpy(name, prox_port_cfg[port_id].driver_name, 31);
		if ((ptr = strstr(name, "_pmd")) != NULL) {
			*ptr = '\x0';
		}
		if (strncmp(name, "rte_", 4) == 0) {
			mvwaddstrf(win_stat, 2 + count, 14, "%7s", name+4);
		} else {
			mvwaddstrf(win_stat, 2 + count, 14, "%7s", name);
		}
		count++;
	}
	display_unlock();
}

static void stats_display_core_ports(unsigned chosen_page)
{
	const uint32_t n_tasks_tot = stats_get_n_tasks_tot();

	display_lock();
	wbkgdset(win_stat, COLOR_PAIR(YELLOW_ON_NOTHING));
	if (stats_cpu_freq_enabled()) {
		col_offset = 20;
	}
	/* Sub-section separator lines */
	mvwvline(win_stat, 1,  4,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 13,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 33,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 53,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 63,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 73,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 85,  ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 97,  ACS_VLINE, n_tasks_tot + 1);
	if (stats_cpu_freq_enabled()) {
		mvwvline(win_stat, 1, 109,  ACS_VLINE, n_tasks_tot + 1);
		mvwvline(win_stat, 1, 119,  ACS_VLINE, n_tasks_tot + 1);
	}
	mvwvline(win_stat, 1, 124 + col_offset, ACS_VLINE, n_tasks_tot + 1);
	mvwvline(win_stat, 1, 139 + col_offset, ACS_VLINE, n_tasks_tot + 1);
	if (stats_cqm_enabled()) {
		mvwvline(win_stat, 1, 154 + col_offset, ACS_VLINE, n_tasks_tot + 1);
	}

	wattron(win_stat, A_BOLD);
	/* Section separators (bold) */
	mvwvline(win_stat, 0, 23, ACS_VLINE, n_tasks_tot + 2);
	mvwvline(win_stat, 0, 44, ACS_VLINE, n_tasks_tot + 2);
	mvwvline(win_stat, 0, 109 + col_offset, ACS_VLINE, n_tasks_tot + 2);
	if (stats_cqm_enabled()) {
		mvwvline(win_stat, 0, 119 + col_offset, ACS_VLINE, n_tasks_tot + 2);
	}

	/* Labels */
	mvwaddstrf(win_stat, 0, 8,   "Core/Task");
	mvwaddstrf(win_stat, 1, 0,   "  Nb");
	mvwaddstrf(win_stat, 1, 5,   "Name");
	mvwaddstrf(win_stat, 1, 14,  "Mode     ");

	mvwaddstrf(win_stat, 0, 24, " Port ID/Ring Name");
	mvwaddstrf(win_stat, 1, 24, "       RX");
	mvwaddstrf(win_stat, 1, 34, "        TX");

	if (!stats_cpu_freq_enabled()) {
		mvwaddstrf(win_stat, 0, 45, "        Statistics per second         ");
	}
	else {
		mvwaddstrf(win_stat, 0, 45, "                  Statistics per second                   ");
	}
	mvwaddstrf(win_stat, 1, 45, "%s", "Idle (%)");
	mvwaddstrf(win_stat, 1, 54, "   RX (k)");
	mvwaddstrf(win_stat, 1, 64, "   TX (k)");
	mvwaddstrf(win_stat, 1, 74, "TX Fail (k)");
	mvwaddstrf(win_stat, 1, 86, "Discard (k)");
	mvwaddstrf(win_stat, 1, 98, "Handled (k)");
	if (stats_cpu_freq_enabled()) {
		mvwaddstrf(win_stat, 1, 110, "      CPP");
		mvwaddstrf(win_stat, 1, 120, "Clk (GHz)");
	}

	mvwaddstrf(win_stat, 0, 110 + col_offset, "              Total Statistics             ");
	mvwaddstrf(win_stat, 1, 110 + col_offset, "            RX");
	mvwaddstrf(win_stat, 1, 125 + col_offset, "            TX");
	mvwaddstrf(win_stat, 1, 140 + col_offset, "          Drop");

	if (stats_cqm_enabled()) {
		mvwaddstrf(win_stat, 0, 155 + col_offset, "  Cache QoS Monitoring  ");
		mvwaddstrf(win_stat, 1, 155 + col_offset, "occupancy (KB)");
		mvwaddstrf(win_stat, 1, 170 + col_offset, " fraction");
	}
	wattroff(win_stat, A_BOLD);
	wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));

	uint16_t line_no = 0;
	uint32_t lcore_id = -1;
	while(prox_core_next(&lcore_id, 0) == 0) {
		const struct lcore_cfg *const cur_core = &lcore_cfg[lcore_id];

		for (uint8_t task_id = 0; task_id < cur_core->n_tasks_all; ++task_id) {
			const struct task_args *const targ = &cur_core->targs[task_id];

			if (line_no >= core_port_height * chosen_page && line_no < core_port_height * (chosen_page + 1)) {

				if (cur_core->n_tasks_run == 0) {
					wattron(win_stat, A_BOLD);
					wbkgdset(win_stat, COLOR_PAIR(RED_ON_NOTHING));
				}
				if (task_id == 0)
					mvwaddstrf(win_stat, line_no % core_port_height + 2, 0, "%2u/", lcore_id);
				if (cur_core->n_tasks_run == 0) {
					wattroff(win_stat, A_BOLD);
					wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
				}

				// Core number and name
				if (!lconf_task_is_running(cur_core, task_id)) {
					wattron(win_stat, A_BOLD);
					wbkgdset(win_stat, COLOR_PAIR(RED_ON_NOTHING));
				}
				mvwaddstrf(win_stat, line_no % core_port_height + 2, 3, "%1u", task_id);

				if (!lconf_task_is_running(cur_core, task_id)) {
					wattroff(win_stat, A_BOLD);
					wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
				}
				mvwaddstrf(win_stat, line_no % core_port_height + 2, 5, "%s", task_id == 0 ? cur_core->name : "");
				mvwaddstrf(win_stat, line_no % core_port_height + 2, 14, "%.9s", targ->task_init->mode_str);
				if (strlen(targ->task_init->mode_str) > 9)
					mvwaddstrf(win_stat, line_no % core_port_height + 2, 22 , "~");
				// Rx port information
				if (targ->nb_rxrings == 0) {
					uint32_t pos_offset = 24;

					for (int i = 0; i < targ->nb_rxports; i++) {
						wbkgdset(win_stat, link_color(targ->rx_ports[i]));
						pos_offset += mvwaddstrf(win_stat, line_no % core_port_height + 2, pos_offset, "%u", targ->rx_ports[i]);
						wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
						/* Space between ports */
						if (i != targ->nb_rxports -1)
							pos_offset++;
						if (pos_offset - 24 >= 9)
							break;
					}
				}
				uint8_t ring_id;
				for (ring_id = 0; ring_id < targ->nb_rxrings && ring_id < 9; ++ring_id) {
					mvwaddstrf(win_stat, line_no % core_port_height + 2, 24 + ring_id, "%s", targ->rx_rings[ring_id]->name);
				}
				if (ring_id == 9 && ring_id < targ->nb_rxrings) {
					mvwaddstrf(win_stat, line_no % core_port_height + 2, 24 + ring_id -1 , "~");
				}
				// Tx port information
				uint8_t pos = 34;
				for (uint8_t i = 0; i < targ->nb_txports; ++i) {
					if (i) {
						if (pos - 34 >= 9) {
							mvwaddstrf(win_stat, line_no % core_port_height + 2, pos -1, "~");
							break;
						}
						++pos;
					}

					if (pos - 34 >= 10) {
						mvwaddstrf(win_stat, line_no % core_port_height + 2, pos -1, "~");
						break;
					}
					wbkgdset(win_stat, link_color(targ->tx_port_queue[i].port));
					mvwaddstrf(win_stat, line_no % core_port_height + 2, pos, "%u", targ->tx_port_queue[i].port);
					wbkgdset(win_stat, COLOR_PAIR(NO_COLOR));
					pos++;
				}
				for (ring_id = 0; ring_id < targ->nb_txrings && ring_id < 10; ++ring_id) {
					mvwaddstrf(win_stat, line_no % core_port_height + 2, pos + ring_id, "%s", targ->tx_rings[ring_id]->name);
				}
				if (ring_id == 10 && ring_id < targ->nb_txrings)
					mvwaddstrf(win_stat, line_no % core_port_height + 2, pos + ring_id-1, "~");
			}
			PROX_ASSERT(line_no < RTE_MAX_LCORE*MAX_TASKS_PER_CORE);

			task_stats_disp[line_no].lcore_id = lcore_id;
			task_stats_disp[line_no].task_id = task_id;
			task_stats_disp[line_no].lcore_stat_id = stats_lcore_find_stat_id(lcore_id);
			line_no++;
		}
	}
	display_unlock();
}

static void redraw_tabs(unsigned screen_id)
{
	const char* views[] = {
		"tasks  ",
		"ports  ",
		"mem    ",
		"lat    ",
		"ring   ",
		"l4gen  ",
		"pkt len",
	};
	const size_t len = 7;

	for (unsigned i = 0; i < sizeof(views)/sizeof(views[0]); ++i) {
		if (i == screen_id)
			wbkgdset(win_tabs, COLOR_PAIR(BLACK_ON_GREEN));

		mvwaddstrf(win_tabs, 0, i*(len + 3), "%u ", i+1);
		if (i != screen_id)
			wbkgdset(win_tabs, COLOR_PAIR(GREEN_ON_NOTHING));
		mvwaddstrf(win_tabs, 0, i*(len + 3) + 2, "%s", views[i]);
		if (i != screen_id)
			wbkgdset(win_tabs, COLOR_PAIR(NO_COLOR));
		if (i == screen_id)
			wbkgdset(win_tabs, COLOR_PAIR(NO_COLOR));
	}

	wrefresh(win_tabs);
}

static void stats_display_layout(uint8_t in_place)
{
	uint8_t cur_stats_height;

	switch (screen_state.chosen_screen) {
	case 0:
		cur_stats_height = core_port_height;
		break;
	case 1:
		cur_stats_height = stats_get_n_ports();
		break;
	case 2:
		cur_stats_height = stats_get_n_mempools();
		break;
	case 3:
		cur_stats_height = stats_get_n_latency();
		break;
	case 4:
		cur_stats_height = stats_get_n_rings();
		break;
	case 5:
		cur_stats_height = stats_get_n_l4gen();
		break;
	case 6:
		cur_stats_height = stats_get_n_ports();
		break;
	default:
		cur_stats_height = core_port_height;
	}

	cur_stats_height = cur_stats_height > max_n_lines? max_n_lines: cur_stats_height;

	display_lock();
	if (!in_place) {
		// moving existing windows does not work
		delwin(win_txt);
		delwin(win_general);
		delwin(win_title);
		delwin(win_tabs);
		delwin(win_cmd);
		delwin(win_txt);
		delwin(win_help);

		clear();
	}

	if (!in_place) {
		win_stat = create_subwindow(cur_stats_height + 2, 0, 4, 0);
		win_tabs = create_subwindow(1, 0, 1, 0);
		win_general = create_subwindow(2, 0, 2, 0);
		win_title = create_subwindow(1, 0, 0, 0);
		win_cmd = create_subwindow(1, 0, cur_stats_height + 2 + 4,  0);
		win_txt_height = LINES - cur_stats_height - 2 - 3 - 3;
		win_txt = create_subwindow(win_txt_height, 0, cur_stats_height + 4 + 3, 0);
		win_help = create_subwindow(1, 0, LINES - 1, 0);
	}
	/* Title box */
	wbkgd(win_title, COLOR_PAIR(BLACK_ON_GREEN));

	char title_str[128];

	redraw_tabs(screen_state.chosen_screen);
	snprintf(title_str, sizeof(title_str), "%s %s: %s", PROGRAM_NAME, VERSION_STR, prox_cfg.name);
	title_len = strlen(title_str);
	mvwaddstrf(win_title, 0, (COLS - title_len)/2, "%s", title_str);

	wattron(win_general, A_BOLD);
	wbkgdset(win_general, COLOR_PAIR(MAGENTA_ON_NOTHING));
	mvwaddstrf(win_general, 0, 9, "rx:         tx:          diff:                     rx:          tx:                        %%:");
	mvwaddstrf(win_general, 1, 9, "rx:         tx:          err:                      rx:          tx:          err:          %%:");
	wbkgdset(win_general, COLOR_PAIR(NO_COLOR));


	wbkgdset(win_general, COLOR_PAIR(BLUE_ON_NOTHING));
	mvwaddstrf(win_general, 0, 0, "Host pps ");
	mvwaddstrf(win_general, 1, 0, "NICs pps ");

	wbkgdset(win_general, COLOR_PAIR(CYAN_ON_NOTHING));
	mvwaddstrf(win_general, 0, 56, "avg");
	mvwaddstrf(win_general, 1, 56, "avg");
	wbkgdset(win_general, COLOR_PAIR(NO_COLOR));
	wattroff(win_general, A_BOLD);

	/* Command line */
	wbkgd(win_cmd, COLOR_PAIR(BLACK_ON_YELLOW));
	idlok(win_cmd, FALSE);
	/* Move cursor at insertion point */
	leaveok(win_cmd, FALSE);

	/* Help/status bar */
	wbkgd(win_help, COLOR_PAIR(BLACK_ON_WHITE));
	werase(win_help);
	waddstr(win_help, "Enter 'help' or command, <ESC> or 'quit' to exit, F1-F5 or 1-5 to switch screens and 0 to reset stats");
	wrefresh(win_help);
	mvwin(win_help, LINES - 1, 0);
	/* Log window */
	idlok(win_txt, FALSE);
	/* Get scrolling */
	scrollok(win_txt, TRUE);

	/* Leave cursor where it was */
	leaveok(win_txt, TRUE);

	wbkgd(win_txt, COLOR_PAIR(BLACK_ON_CYAN));
	wrefresh(win_txt);

	/* Draw everything to the screen */
	refresh();
	display_unlock();


	switch (screen_state.chosen_screen) {
	case 0:
		stats_display_core_ports(screen_state.chosen_page);
		break;
	case 1:
		stats_display_eth_ports();
		break;
	case 2:
		stats_display_mempools();
		break;
	case 3:
		stats_display_latency();
		break;
	case 4:
		stats_display_rings();
		break;
	case 5:
		stats_display_l4gen();
		break;
	case 6:
		stats_display_pkt_len();
		break;
	}

	refresh_cmd_win();
	display_stats();
}

void display_end(void)
{
	pthread_mutex_destroy(&disp_mtx);

	if (scr != NULL) {
		endwin();
	}
}

static void display_core_task_stats(uint8_t task_id)
{
	const int line_no = task_id % core_port_height;
	const struct task_stats_disp *t = &task_stats_disp[task_id];
	struct task_stats_sample *last = stats_get_task_stats_sample(t->lcore_id, t->task_id, 1);
	struct task_stats_sample *prev = stats_get_task_stats_sample(t->lcore_id, t->task_id, 0);

	/* delta_t in units of clock ticks */
	uint64_t delta_t = last->tsc - prev->tsc;

	uint64_t empty_cycles = last->empty_cycles - prev->empty_cycles;

	if (empty_cycles > delta_t) {
		empty_cycles = 10000;
	}
	else {
		empty_cycles = empty_cycles * 10000 / delta_t;
	}

	// empty_cycles has 2 digits after point, (usefull when only a very small idle time)
	mvwaddstrf(win_stat, line_no + 2, 47, "%3lu.%02lu", empty_cycles / 100, empty_cycles % 100);

	// Display per second statistics in Kpps unit
	if (prox_cfg.pps_unit)
		delta_t *= prox_cfg.pps_unit;
	else
		delta_t *= 1000;

	uint64_t nb_pkt;
	nb_pkt = (last->rx_pkt_count - prev->rx_pkt_count) * tsc_hz;
	if (nb_pkt && nb_pkt < delta_t) {
		mvwaddstrf(win_stat, line_no + 2, 54, "    0.%03lu", nb_pkt * 1000 / delta_t);
	}
	else {
		mvwaddstrf(win_stat, line_no + 2, 54, "%9lu", nb_pkt / delta_t);
	}

	nb_pkt = (last->tx_pkt_count - prev->tx_pkt_count) * tsc_hz;
	if (nb_pkt && nb_pkt < delta_t) {
		mvwaddstrf(win_stat, line_no + 2, 64, "    0.%03lu", nb_pkt * 1000 / delta_t);
	}
	else {
		mvwaddstrf(win_stat, line_no + 2, 64, "%9lu", nb_pkt / delta_t);
	}

	nb_pkt = (last->drop_tx_fail - prev->drop_tx_fail) * tsc_hz;
	if (nb_pkt && nb_pkt < delta_t) {
		mvwaddstrf(win_stat, line_no + 2, 76, "    0.%03lu", nb_pkt * 1000 / delta_t);
	}
	else {
		mvwaddstrf(win_stat, line_no + 2, 76, "%9lu", nb_pkt / delta_t);
	}

	nb_pkt = (last->drop_discard - prev->drop_discard) * tsc_hz;
	if (nb_pkt && nb_pkt < delta_t) {
		mvwaddstrf(win_stat, line_no + 2, 88, "    0.%03lu", nb_pkt * 1000 / delta_t);
	}
	else {
		mvwaddstrf(win_stat, line_no + 2, 88, "%9lu", nb_pkt / delta_t);
	}

	nb_pkt = (last->drop_handled - prev->drop_handled) * tsc_hz;
	if (nb_pkt && nb_pkt < delta_t) {
		mvwaddstrf(win_stat, line_no + 2, 100, "    0.%03lu", nb_pkt * 1000 / delta_t);
	}
	else {
		mvwaddstrf(win_stat, line_no + 2, 100, "%9lu", nb_pkt / delta_t);
	}

	if (stats_cpu_freq_enabled()) {
		uint8_t lcore_stat_id = t->lcore_stat_id;
		struct lcore_stats_sample *clast = stats_get_lcore_stats_sample(lcore_stat_id, 1);
		struct lcore_stats_sample *cprev = stats_get_lcore_stats_sample(lcore_stat_id, 0);

		uint64_t adiff = clast->afreq - cprev->afreq;
		uint64_t mdiff = clast->mfreq - cprev->mfreq;

		if ((last->rx_pkt_count - prev->rx_pkt_count) && mdiff) {
			mvwaddstrf(win_stat, line_no + 2, 110, "%9lu", delta_t/(last->rx_pkt_count - prev->rx_pkt_count)*adiff/mdiff/1000);
		}
		else {
			mvwaddstrf(win_stat, line_no + 2, 110, "%9lu", 0L);
		}

		uint64_t mhz;
		if (mdiff)
			mhz = tsc_hz*adiff/mdiff/1000000;
		else
			mhz = 0;

		mvwaddstrf(win_stat, line_no + 2, 120, "%5lu.%03lu", mhz/1000, mhz%1000);
	}

	struct task_stats *ts = stats_get_task_stats(t->lcore_id, t->task_id);

	// Total statistics (packets)
	mvwaddstrf(win_stat, line_no + 2, 110 + col_offset, "%14lu", ts->tot_rx_pkt_count);
	mvwaddstrf(win_stat, line_no + 2, 125 + col_offset, "%14lu", ts->tot_tx_pkt_count);
	mvwaddstrf(win_stat, line_no + 2, 140 + col_offset, "%14lu", ts->tot_drop_tx_fail +
		   ts->tot_drop_discard + ts->tot_drop_handled);

	if (stats_cqm_enabled()) {
		struct lcore_stats *c = stats_get_lcore_stats(t->lcore_stat_id);

		mvwaddstrf(win_stat, line_no + 2, 155 + col_offset, "%14lu", c->cqm_bytes >> 10);
		mvwaddstrf(win_stat, line_no + 2, 170 + col_offset, "%6lu.%02lu", c->cqm_fraction/100, c->cqm_fraction%100);
	}
}

static void pps_print(WINDOW *dst_scr, int y, int x, uint64_t val, int is_blue)
{
	uint64_t rx_pps_disp = val;
	uint64_t rx_pps_disp_frac = 0;
	uint32_t ten_pow3 = 0;
	static const char *units = " KMG";
	char rx_unit = ' ';

	while (rx_pps_disp > 1000) {
		rx_pps_disp /= 1000;
		rx_pps_disp_frac = (val - rx_pps_disp*1000) / 10;
		val /= 1000;
		ten_pow3++;
	}

	if (ten_pow3 >= strlen(units)) {
		wbkgdset(dst_scr, COLOR_PAIR(RED_ON_NOTHING));
		mvwaddstrf(dst_scr, y, x, "---");
		wbkgdset(dst_scr, COLOR_PAIR(NO_COLOR));
		return;
	}

	rx_unit = units[ten_pow3];

	wattron(dst_scr, A_BOLD);
	if (is_blue) {
		wbkgdset(dst_scr, COLOR_PAIR(BLUE_ON_NOTHING));
	}
	else
		wbkgdset(dst_scr, COLOR_PAIR(CYAN_ON_NOTHING));

	mvwaddstrf(dst_scr, y, x, "%3lu", rx_pps_disp);
	if (rx_unit != ' ') {
		mvwaddstrf(dst_scr, y, x + 3, ".%02lu", rx_pps_disp_frac);
		wattroff(dst_scr, A_BOLD);
		wbkgdset(dst_scr, COLOR_PAIR(WHITE_ON_NOTHING));
		wattron(dst_scr, A_BOLD);
		mvwaddstrf(dst_scr, y, x + 6, "%c", rx_unit);
		wattroff(dst_scr, A_BOLD);
		wbkgdset(dst_scr, COLOR_PAIR(NO_COLOR));
	}
	else {
		mvwaddstrf(dst_scr, y, x + 3, "    ");
	}
	wattroff(dst_scr, A_BOLD);
	wbkgdset(dst_scr, COLOR_PAIR(NO_COLOR));
}

static void display_stats_general(void)
{
	/* moment when stats were gathered. */
	uint64_t cur_tsc = stats_get_last_tsc();
	uint64_t up_time = (cur_tsc - stats_global_start_tsc())/tsc_hz;
	uint64_t up_time2 = (cur_tsc - stats_global_beg_tsc())/tsc_hz;
	uint64_t rem_time = -1;
	char title_str[128] = {0};

	if (stats_global_end_tsc())
		rem_time = stats_global_end_tsc() > cur_tsc? (stats_global_end_tsc() - cur_tsc)/tsc_hz : 0;

	if (up_time != up_time2 && cur_tsc >= stats_global_beg_tsc()) {
		if (stats_global_end_tsc())
			snprintf(title_str, sizeof(title_str), "%5lu (%lu) up, %lu rem", up_time, up_time2, rem_time);
		else
			snprintf(title_str, sizeof(title_str), "%5lu (%lu) up", up_time, up_time2);
	}
	else {
		if (stats_global_end_tsc())
			snprintf(title_str, sizeof(title_str), "%5lu up, %lu rem", up_time, rem_time);
		else
			snprintf(title_str, sizeof(title_str), "%5lu up", up_time);
	}

	/* Only print up time information if there is enough space */
	if ((int)((COLS + title_len)/2 + strlen(title_str) + 1) < COLS) {
		mvwaddstrf(win_title, 0, COLS - strlen(title_str), "%s", title_str);
		wrefresh(win_title);
	}

	struct global_stats_sample *gsl = stats_get_global_stats(1);
	struct global_stats_sample *gsp = stats_get_global_stats(0);

	uint64_t rx_pps = div_round(gsl->host_rx_packets - gsp->host_rx_packets, gsl->tsc - gsp->tsc);
	uint64_t tx_pps = div_round(gsl->host_tx_packets - gsp->host_tx_packets, gsl->tsc - gsp->tsc);
	/* Host: RX, TX, Diff */
	pps_print(win_general, 0, 12, rx_pps, 1);
	pps_print(win_general, 0, 25, tx_pps, 1);

	uint64_t diff = 0;
	if (rx_pps > tx_pps)
		diff = rx_pps - tx_pps;
	pps_print(win_general, 0, 40, diff, 1);

	uint64_t nics_rx_pps = div_round(gsl->nics_rx_packets - gsp->nics_rx_packets, gsl->tsc - gsp->tsc);
	uint64_t nics_tx_pps = div_round(gsl->nics_tx_packets - gsp->nics_tx_packets, gsl->tsc - gsp->tsc);
	uint64_t nics_ierrors = div_round(gsl->nics_ierrors - gsp->nics_ierrors, gsl->tsc - gsp->tsc);

	/* NIC: RX, TX, Diff */
	pps_print(win_general, 1, 12, nics_rx_pps, 1);
	pps_print(win_general, 1, 25, nics_tx_pps, 1);
	pps_print(win_general, 1, 40, nics_ierrors, 1);

	wbkgdset(win_general, COLOR_PAIR(CYAN_ON_NOTHING));
	wattron(win_general, A_BOLD);
	mvwaddstrf(win_general, 0, 103, "%6.2f", tx_pps > rx_pps? 100 : tx_pps * 100.0 / rx_pps);
	wattroff(win_general, A_BOLD);
	wbkgdset(win_general, COLOR_PAIR(NO_COLOR));

	struct global_stats_sample *gsb = stats_get_global_stats_beg();
	if (gsb) {
		uint64_t rx_pps = div_round(gsl->host_rx_packets - gsb->host_rx_packets, gsl->tsc - gsb->tsc);
		uint64_t tx_pps = div_round(gsl->host_tx_packets - gsb->host_tx_packets, gsl->tsc - gsb->tsc);

		uint64_t nics_rx_pps = div_round(gsl->nics_rx_packets - gsb->nics_rx_packets, gsl->tsc - gsb->tsc);
		uint64_t nics_tx_pps = div_round(gsl->nics_tx_packets - gsb->nics_tx_packets, gsl->tsc - gsb->tsc);
		uint64_t nics_ierrors = div_round(gsl->nics_ierrors - gsb->nics_ierrors, gsl->tsc - gsb->tsc);

		pps_print(win_general, 0, 64, rx_pps, 0);
		pps_print(win_general, 0, 77, tx_pps, 0);

		pps_print(win_general, 1, 64, nics_rx_pps, 0);
		pps_print(win_general, 1, 77, nics_tx_pps, 0);
		pps_print(win_general, 1, 91, nics_ierrors, 0);

		wbkgdset(win_general, COLOR_PAIR(CYAN_ON_NOTHING));
		wattron(win_general, A_BOLD);
		uint64_t nics_in = gsl->host_rx_packets - gsb->host_rx_packets + gsl->nics_ierrors - gsb->nics_ierrors;
		uint64_t nics_out = gsl->host_tx_packets - gsb->host_tx_packets;
		mvwaddstrf(win_general, 1, 103, "%6.2f", nics_out > nics_in?
			   100 : nics_out * 100.0 / nics_in);
		wattron(win_general, A_BOLD);
		wbkgdset(win_general, COLOR_PAIR(NO_COLOR));
	}

	wrefresh(win_general);

	wattroff(win_stat, A_BOLD);
}

static void display_stats_core_ports(void)
{
	unsigned chosen_page = screen_state.chosen_page;
	const uint32_t n_tasks_tot = stats_get_n_tasks_tot();

	uint32_t beg = core_port_height * chosen_page;
	uint32_t end = n_tasks_tot < core_port_height * (chosen_page + 1)? n_tasks_tot : core_port_height * (chosen_page + 1);

	for (uint8_t active_core = beg; active_core < end; ++active_core) {
		display_core_task_stats(active_core);
	}
}

static void display_stats_eth_ports(void)
{
	uint8_t count = 0;

	for (uint8_t i = 0; i < n_port_disp; ++i) {
		const uint32_t port_id = port_disp[i];
		struct port_stats_sample *last = stats_get_port_stats_sample(port_id, 1);
		struct port_stats_sample *prev = stats_get_port_stats_sample(port_id, 0);

		uint64_t delta_t = last->tsc - prev->tsc;
		if (delta_t == 0) // This could happen if we just reset the screen => stats will be updated later
			continue;
		uint64_t thresh = UINT64_MAX/tsc_hz;

		uint64_t no_mbufs_diff = last->no_mbufs - prev->no_mbufs;
		uint64_t ierrors_diff = last->ierrors - prev->ierrors;
		uint64_t oerrors_diff = last->oerrors - prev->oerrors;

		uint64_t rx_bytes_diff = last->rx_bytes - prev->rx_bytes;
		uint64_t tx_bytes_diff = last->tx_bytes - prev->tx_bytes;

		uint64_t rx_diff = last->rx_tot - prev->rx_tot;
		uint64_t tx_diff = last->tx_tot - prev->tx_tot;
		uint64_t rx_percent = rx_bytes_diff;
		uint64_t tx_percent = tx_bytes_diff;

		if (no_mbufs_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 22, "%12lu", no_mbufs_diff*tsc_hz/delta_t);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 22, "%12lu", no_mbufs_diff/(delta_t/tsc_hz));
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 22, "%12s", "---");
		}

		if (ierrors_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 35, "%11lu", ierrors_diff*tsc_hz/delta_t);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 35, "%11lu", ierrors_diff/(delta_t/tsc_hz));
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 35, "%11s", "---");
		}

		if (oerrors_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 47, "%11lu", oerrors_diff*tsc_hz/delta_t);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 47, "%11lu", oerrors_diff/(delta_t/tsc_hz));
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 47, "%11s", "---");
		}

		if (rx_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 47 + 12, "%9lu", (rx_diff*tsc_hz/delta_t)/1000);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 47 + 12, "%9lu", (rx_diff/(delta_t/tsc_hz))/1000);
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 47 + 12, "%9s", "---");
		}

		if (tx_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 57 + 12, "%9lu", (tx_diff*tsc_hz/delta_t)/1000);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 57 + 12, "%9lu", (tx_diff/(delta_t/tsc_hz))/1000);
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 57 + 12, "%9s", "---");
		}

		if (rx_bytes_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 67 + 12, "%9lu", (rx_bytes_diff*tsc_hz/delta_t)/125);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 67 + 12, "%9lu", (rx_bytes_diff/(delta_t/tsc_hz))/125);
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 67 + 12 , "%9s", "---");
		}

		if (tx_bytes_diff < thresh) {
			mvwaddstrf(win_stat, 2 + count, 77 + 12, "%9lu", (tx_bytes_diff*tsc_hz/delta_t)/125);
		}
		else if (delta_t > tsc_hz) {
			mvwaddstrf(win_stat, 2 + count, 77 + 12, "%9lu", (tx_bytes_diff/(delta_t/tsc_hz))/125);
		}
		else {
			mvwaddstrf(win_stat, 2 + count, 77 + 12, "%9s", "---");
		}


		if (rx_percent) {
			if (rx_percent < thresh) {
				mvwaddstrf(win_stat, 2 + count, 87 + 12, "%3lu.%04lu", rx_percent * tsc_hz / delta_t / 12500000, (rx_percent * tsc_hz / delta_t / 1250) % 10000);
			}
			else if (delta_t > tsc_hz) {
				mvwaddstrf(win_stat, 2 + count, 87 + 12, "%3lu.%04lu", rx_percent / (delta_t /tsc_hz)/ 12500000, (rx_percent /(delta_t /tsc_hz) / 1250) % 10000);
			}
			else {
				mvwaddstrf(win_stat, 2 + count, 87 + 12, "%9s", "---");
			}
		}
		else
			mvwaddstrf(win_stat, 2 + count, 87 + 12, "%8u", 0);

		if (tx_percent) {
			if (tx_percent < thresh) {
				mvwaddstrf(win_stat, 2 + count, 96 + 12, "%3lu.%04lu", tx_percent * tsc_hz / delta_t / 12500000, (tx_percent * tsc_hz / delta_t / 1250) % 10000);
			}
			else if (delta_t > tsc_hz) {
				mvwaddstrf(win_stat, 2 + count, 96 + 12, "%3lu.%04lu", tx_percent / (delta_t /tsc_hz)/ 12500000, (tx_percent /(delta_t /tsc_hz) / 1250) % 10000);
			}
			else {
				mvwaddstrf(win_stat, 2 + count, 96 + 12, "%9s", "---");
			}
		}
		else
			mvwaddstrf(win_stat, 2 + count, 96 + 12, "%8u", 0);
		mvwaddstrf(win_stat, 2 + count, 105 + 12, "%13lu", last->rx_tot);
		mvwaddstrf(win_stat, 2 + count, 119 + 12, "%13lu", last->tx_tot);

		mvwaddstrf(win_stat, 2 + count, 133 + 12, "%13lu", last->no_mbufs);
		mvwaddstrf(win_stat, 2 + count, 147 + 12, "%13lu", last->ierrors);
		mvwaddstrf(win_stat, 2 + count, 173, "%13lu", last->oerrors);
		count++;
	}
}

static void display_stats_mempools(void)
{
	const uint32_t n_mempools = stats_get_n_mempools();

	for (uint16_t i = 0; i < n_mempools; ++i) {
		struct mempool_stats *ms = stats_get_mempool_stats(i);
		const size_t used = ms->size - ms->free;
		const uint32_t used_frac = used*10000/ms->size;

		mvwaddstrf(win_stat, 2 + i, 14, "%3u.%02u", used_frac/100, used_frac % 100);
		mvwaddstrf(win_stat, 2 + i, 21, "%12zu", used);
		mvwaddstrf(win_stat, 2 + i, 34, "%12zu", ms->free);
		mvwaddstrf(win_stat, 2 + i, 60, "%14zu", used * MBUF_SIZE/1024);
		mvwaddstrf(win_stat, 2 + i, 75, "%14zu", ms->free * MBUF_SIZE/1024);
	}
}

static void display_stats_rings(void)
{
	const uint32_t n_rings = stats_get_n_rings() > max_n_lines? max_n_lines : stats_get_n_rings();
	int top = 2;
	int left = 0;
	uint32_t used;

	for (uint32_t i = 0; i < n_rings; ++i) {
		struct ring_stats *rs = stats_get_ring_stats(i);

		left = 0;
		used = ((rs->size - rs->free)*10000)/rs->size;
		left += 24;
		mvwaddstrf(win_stat, top, left, "%8u.%02u", used/100, used%100);
		left += 12;
		mvwaddstrf(win_stat, top, left, "%9u", rs->free);
		left += 10;
		mvwaddstrf(win_stat, top, left, "%9u", rs->size);
		top += rs->nb_ports ? rs->nb_ports : 1;
	}
}

static void display_stats_l4gen(void)
{
	const uint32_t n_l4gen = stats_get_n_l4gen();

	const uint64_t hz = rte_get_tsc_hz();

	for (uint16_t i = 0; i < n_l4gen; ++i) {
		struct l4_stats_sample *clast = stats_get_l4_stats_sample(i, 1);
		struct l4_stats_sample *cprev = stats_get_l4_stats_sample(i, 0);

		struct l4_stats *last = &clast->stats;
		struct l4_stats *prev = &cprev->stats;

		uint64_t delta_t = clast->tsc - cprev->tsc;

		uint64_t tcp_created = last->tcp_created - prev->tcp_created;
		uint64_t udp_created = last->udp_created - prev->udp_created;

		uint64_t tcp_finished_no_retransmit = last->tcp_finished_no_retransmit - prev->tcp_finished_no_retransmit;
		uint64_t tcp_finished_retransmit = last->tcp_finished_retransmit - prev->tcp_finished_retransmit;
		uint64_t tcp_expired = last->tcp_expired - prev->tcp_expired;
		uint64_t tcp_retransmits = last->tcp_retransmits - prev->tcp_retransmits;
		uint64_t udp_finished = last->udp_finished - prev->udp_finished;
		uint64_t udp_expired = last->udp_expired - prev->udp_expired;
		uint64_t bundles_created = last->bundles_created - prev->bundles_created;

		mvwaddstrf(win_stat, 2 + i, 5, "%9"PRIu64"", tcp_created*hz/delta_t);
		mvwaddstrf(win_stat, 2 + i, 15, "%9"PRIu64"", udp_created*hz/delta_t);
		mvwaddstrf(win_stat, 2 + i, 25, "%9"PRIu64"", tcp_created*hz/delta_t + udp_created*hz/delta_t);
		mvwaddstrf(win_stat, 2 + i, 35, "%11"PRIu64"", bundles_created*hz/delta_t);

		mvwaddstrf(win_stat, 2 + i, 35 + 12, "%12"PRIu64"", tcp_finished_no_retransmit*hz/delta_t);
		mvwaddstrf(win_stat, 2 + i, 48 + 12, "%12"PRIu64"", tcp_finished_retransmit*hz/delta_t);
		mvwaddstrf(win_stat, 2 + i, 61 + 12, "%12"PRIu64"", udp_finished*hz/delta_t);

		mvwaddstrf(win_stat, 2 + i, 74 + 12, "%10"PRIu64"", tcp_expired*hz/delta_t);
		mvwaddstrf(win_stat, 2 + i, 85 + 12, "%10"PRIu64"", udp_expired*hz/delta_t);

		uint64_t tot_created = last->tcp_created + last->udp_created;
		uint64_t tot_finished = last->tcp_finished_retransmit + last->tcp_finished_no_retransmit +
			last->udp_finished + last->udp_expired + last->tcp_expired;

		mvwaddstrf(win_stat, 2 + i, 96 + 12, "%10"PRIu64"",  tot_created - tot_finished);
		mvwaddstrf(win_stat, 2 + i, 107 + 12, "%10"PRIu64"", tcp_retransmits*hz/delta_t);
	}
}

static void display_stats_pkt_len(void)
{
	uint8_t count = 0;

	for (uint8_t i = 0; i < n_port_disp; ++i) {
		const uint32_t port_id = port_disp[i];
		struct port_stats_sample *last = stats_get_port_stats_sample(port_id, 1);
		struct port_stats_sample *prev = stats_get_port_stats_sample(port_id, 0);

		uint64_t delta_t = last->tsc - prev->tsc;
		if (delta_t == 0) // This could happen if we just reset the screen => stats will be updated later
			continue;

		uint64_t pkt_64 = last->tx_pkt_size[PKT_SIZE_64] - prev->tx_pkt_size[PKT_SIZE_64];
		uint64_t pkt_65 = last->tx_pkt_size[PKT_SIZE_65] - prev->tx_pkt_size[PKT_SIZE_65];
		uint64_t pkt_128 = last->tx_pkt_size[PKT_SIZE_128] - prev->tx_pkt_size[PKT_SIZE_128];
		uint64_t pkt_256 = last->tx_pkt_size[PKT_SIZE_256] - prev->tx_pkt_size[PKT_SIZE_256];
		uint64_t pkt_512 = last->tx_pkt_size[PKT_SIZE_512] - prev->tx_pkt_size[PKT_SIZE_512];
		uint64_t pkt_1024 = last->tx_pkt_size[PKT_SIZE_1024] - prev->tx_pkt_size[PKT_SIZE_1024];

		mvwaddstrf(win_stat, 2 + count, 22, "%13lu", div_round(pkt_64, delta_t));
		mvwaddstrf(win_stat, 2 + count, 37, "%13lu", div_round(pkt_65, delta_t));
		mvwaddstrf(win_stat, 2 + count, 51, "%13lu", div_round(pkt_128, delta_t));
		mvwaddstrf(win_stat, 2 + count, 65, "%13lu", div_round(pkt_256, delta_t));
		mvwaddstrf(win_stat, 2 + count, 79, "%13lu", div_round(pkt_512, delta_t));
		mvwaddstrf(win_stat, 2 + count, 93, "%13lu", div_round(pkt_1024, delta_t));


		mvwaddstrf(win_stat, 2 + count, 107, "%13lu", last->tx_pkt_size[PKT_SIZE_64]);
		mvwaddstrf(win_stat, 2 + count, 121, "%13lu", last->tx_pkt_size[PKT_SIZE_65]);
		mvwaddstrf(win_stat, 2 + count, 135, "%13lu", last->tx_pkt_size[PKT_SIZE_128]);
		mvwaddstrf(win_stat, 2 + count, 149, "%13lu", last->tx_pkt_size[PKT_SIZE_256]);
		mvwaddstrf(win_stat, 2 + count, 163, "%13lu", last->tx_pkt_size[PKT_SIZE_512]);
		mvwaddstrf(win_stat, 2 + count, 177, "%13lu", last->tx_pkt_size[PKT_SIZE_1024]);

		count++;
	}
}

static void display_stats_latency(void)
{
	const uint32_t n_latency = stats_get_n_latency();

	for (uint16_t i = 0; i < n_latency; ++i) {
		struct lat_test *lat_test = stats_get_lat_stats(i);

		if (lat_test->tot_pkts) {
			uint64_t avg_usec, avg_nsec, min_usec, min_nsec, max_usec, max_nsec;

			if ((lat_test->tot_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				avg_usec = (lat_test->tot_lat<<LATENCY_ACCURACY)*1000000/(lat_test->tot_pkts*tsc_hz);
				avg_nsec = ((lat_test->tot_lat<<LATENCY_ACCURACY)*1000000 - avg_usec*lat_test->tot_pkts*tsc_hz)*1000/(lat_test->tot_pkts*tsc_hz);
			}
			else {
				avg_usec = (lat_test->tot_lat<<LATENCY_ACCURACY)/(lat_test->tot_pkts*tsc_hz/1000000);
				avg_nsec = 0;
			}

			if ((lat_test->min_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				min_usec = (lat_test->min_lat<<LATENCY_ACCURACY)*1000000/tsc_hz;
				min_nsec = ((lat_test->min_lat<<LATENCY_ACCURACY)*1000000 - min_usec*tsc_hz)*1000/tsc_hz;
			}
			else {
				min_usec = (lat_test->min_lat<<LATENCY_ACCURACY)/(tsc_hz/1000000);
				min_nsec = 0;
			}


			if ((lat_test->max_lat << LATENCY_ACCURACY) < UINT64_MAX/1000000) {
				max_usec = (lat_test->max_lat<<LATENCY_ACCURACY)*1000000/tsc_hz;
				max_nsec = ((lat_test->max_lat<<LATENCY_ACCURACY)*1000000 - max_usec*tsc_hz)*1000/tsc_hz;
			}
			else {
				max_usec = (lat_test->max_lat<<LATENCY_ACCURACY)/(tsc_hz/1000000);
				max_nsec = 0;
			}

			mvwaddstrf(win_stat, 2 + i, 16, "%6"PRIu64".%03"PRIu64"", min_usec, min_nsec);
			mvwaddstrf(win_stat, 2 + i, 29, "%6"PRIu64".%03"PRIu64"", max_usec, max_nsec);
			mvwaddstrf(win_stat, 2 + i, 42, "%6"PRIu64".%03"PRIu64"", avg_usec, avg_nsec);
			mvwaddstrf(win_stat, 2 + i, 53, "%12.3f", sqrt((((lat_test->var_lat << (2 * LATENCY_ACCURACY)) / lat_test->tot_pkts)*1000000.0/tsc_hz*1000000/tsc_hz) - (((lat_test->tot_lat << LATENCY_ACCURACY) / lat_test->tot_pkts*1000000.0/tsc_hz * (lat_test->tot_lat << LATENCY_ACCURACY) /lat_test->tot_pkts * 1000000/tsc_hz))));
			mvwaddstrf(win_stat, 2 + i, 68, "%14"PRIu64"", lat_test->lost_packets);


			if (lat_test->tot_pkts) {
				mvwaddstrf(win_stat, 2 + i, 140-29, "%11"PRIu64"", lat_test->tot_rx_acc*1000000000/lat_test->tot_pkts/tsc_hz);
			}
			mvwaddstrf(win_stat, 2 + i, 114-29,"%11"PRIu64"", lat_test->min_rx_acc*1000000000/tsc_hz);
			mvwaddstrf(win_stat, 2 + i, 127-29,"%11"PRIu64"", lat_test->max_rx_acc*1000000000/tsc_hz);
			if (lat_test->tot_pkts) {
				mvwaddstrf(win_stat, 2 + i, 179-29, "%11"PRIu64"", lat_test->tot_tx_acc*1000000000/lat_test->tot_pkts/tsc_hz);
			}
			mvwaddstrf(win_stat, 2 + i, 153-29,"%11"PRIu64"", lat_test->min_tx_acc*1000000000/tsc_hz);
			mvwaddstrf(win_stat, 2 + i, 166-29,"%11"PRIu64"", lat_test->max_tx_acc*1000000000/tsc_hz);
		}
	}
}

void display_screen(int screen_id)
{
	if (screen_id < 0 || screen_id > 6) {
		plog_err("Unsupported screen %d\n", screen_id + 1);
		return;
	}

	if (screen_state.chosen_screen == screen_id) {
		stats_display_layout(1);
	}
	else {
		screen_state.chosen_screen = screen_id;
		stats_display_layout(0);
	}
}

void display_page_up(void)
{
	if (screen_state.chosen_page) {
		--screen_state.chosen_page;
		stats_display_layout(0);
	}
}

void display_page_down(void)
{
	if (stats_get_n_tasks_tot() > core_port_height * (screen_state.chosen_page + 1)) {
		++screen_state.chosen_page;
		stats_display_layout(0);
	}
}

void display_refresh(void)
{
	stats_display_layout(1);
}

void display_stats(void)
{
	display_lock();
	switch (screen_state.chosen_screen) {
	case 0:
		display_stats_core_ports();
		break;
	case 1:
		display_stats_eth_ports();
		break;
	case 2:
		display_stats_mempools();
		break;
	case 3:
		display_stats_latency();
		break;
	case 4:
		display_stats_rings();
		break;
	case 5:
		display_stats_l4gen();
		break;
	case 6:
		display_stats_pkt_len();
		break;
	}
	display_stats_general();
	wrefresh(win_stat);
	display_unlock();
}

char pages[32768] = {0};
int cur_idx = 0;
size_t pages_len = 0;

void display_print_page(void)
{
	int n_lines = 0;
	int cur_idx_prev = cur_idx;

	if (cur_idx >= (int)pages_len) {
		return;
	}

	display_lock();
	for (size_t i = cur_idx; i < pages_len; ++i) {
		if (pages[i] == '\n') {
			n_lines++;
			if (n_lines == win_txt_height - 2) {
				pages[i] = 0;
				cur_idx = i + 1;
				break;
			}
		}
	}

	waddstr(win_txt, pages + cur_idx_prev);
	if (cur_idx != cur_idx_prev && cur_idx < (int)pages_len)
		waddstr(win_txt, "\nPRESS ENTER FOR MORE...\n");
	else {
		pages_len = 0;
	}
	wrefresh(win_txt);
	display_unlock();
}

void display_print(const char *str)
{
	display_lock();

	if (scr == NULL) {
		fputs(str, stdout);
		fflush(stdout);
		display_unlock();
		return;
	}

	/* Check if the whole string can fit on the screen. */
	pages_len = strlen(str);
	int n_lines = 0;
	memset(pages, 0, sizeof(pages));
	memcpy(pages, str, pages_len);
	cur_idx = 0;
	for (size_t i = 0; i < pages_len; ++i) {
		if (pages[i] == '\n') {
			n_lines++;
			if (n_lines == win_txt_height - 2) {
				pages[i] = 0;
				cur_idx = i + 1;
				break;
			}
		}
	}

	waddstr(win_txt, pages);
	if (cur_idx != 0)
		waddstr(win_txt, "\nPRESS ENTER FOR MORE...\n");
	else
		pages_len = 0;

	wrefresh(win_txt);
	display_unlock();
}
