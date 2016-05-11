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

#include <inttypes.h>
#include <string.h>

#include <rte_launch.h>
#include <rte_cycles.h>

#include "run.h"
#include "prox_cfg.h"
#include "prox_port_cfg.h"
#include "quit.h"
#include "commands.h"
#include "main.h"
#include "log.h"
#include "display.h"
#include "stats.h"
#include "stats_cons.h"
#include "stats_cons_log.h"

#include "input.h"
#include "input_curses.h"
#include "input_conn.h"

static int needs_refresh;
static uint64_t update_interval;
static int stop_prox = 0; /* set to 1 to stop prox */

void set_update_interval(uint32_t msec)
{
	update_interval = msec_to_tsc(msec);
}

void req_refresh(void)
{
	needs_refresh = 1;
}

void quit(void)
{
	plog_info("Leaving...\n");
	stop_core_all(-1);
	stop_prox = 1;
}

static void update_link_states(void)
{
	struct prox_port_cfg *port_cfg;
	struct rte_eth_link link;

	for (uint8_t portid = 0; portid < PROX_MAX_PORTS; ++portid) {
		if (!prox_port_cfg[portid].active) {
			continue;
		}

		port_cfg  = &prox_port_cfg[portid];
		rte_eth_link_get_nowait(portid, &link);
		port_cfg->link_up = link.link_status;
		port_cfg->link_speed = link.link_speed;
	}
}

static struct stats_cons stats_cons[8];
static size_t n_stats_cons = 0;
static uint8_t stats_cons_flags = 0;

static void stats_cons_add(struct stats_cons *sc)
{
	if (n_stats_cons == sizeof(stats_cons)/sizeof(stats_cons[0]))
		return;

	stats_cons[n_stats_cons++] = *sc;
	sc->init();
	stats_cons_flags |= sc->flags;
}

static void stats_cons_notify(void)
{
	for (size_t i = 0; i < n_stats_cons; ++i) {
		stats_cons[i].notify();
	}
}

static void stats_cons_refresh(void)
{
	for (size_t i = 0; i < n_stats_cons; ++i) {
		if (stats_cons[i].refresh)
			stats_cons[i].refresh();
	}
}

static void stats_cons_finish(void)
{
	for (size_t i = 0; i < n_stats_cons; ++i) {
		if (stats_cons[i].finish)
			stats_cons[i].finish();
	}
}

static void busy_wait_until(uint64_t deadline)
{
	while (rte_rdtsc() < deadline)
		;
}

static void multiplexed_input_stats(uint64_t deadline)
{
	input_proc_until(deadline);

	if (needs_refresh) {
		needs_refresh = 0;
		stats_cons_refresh();
	}

	if (rte_atomic32_read(&lsc)) {
		rte_atomic32_dec(&lsc);
		update_link_states();
		stats_cons_refresh();
	}
}

/* start main loop */
void __attribute__((noreturn)) run(uint32_t flags)
{
	uint64_t cur_tsc;
	uint64_t next_update;
	uint64_t stop_tsc = 0;
	const uint64_t update_interval_threshold = usec_to_tsc(1);

	if (flags & DSF_LISTEN_TCP)
		PROX_PANIC(reg_input_tcp(), "Failed to start listening on TCP port 8474: %s\n", strerror(errno));
	if (flags & DSF_LISTEN_UDS)
		PROX_PANIC(reg_input_uds(), "Failed to start listening on UDS /tmp/prox.sock: %s\n", strerror(errno));

	if (prox_cfg.use_stats_logger)
		stats_cons_add(stats_cons_log_get());

	stats_init(prox_cfg.start_time, prox_cfg.duration_time);
	stats_update(STATS_CONS_F_ALL);


	switch (prox_cfg.ui) {
	case PROX_UI_CURSES:
		reg_input_curses();
		stats_cons_add(&display);
		break;
	case PROX_UI_CLI:
		/* Not implemented */
		break;
	case PROX_UI_NONE:
	default:
		break;
	}

	if (flags & DSF_AUTOSTART)
		start_core_all(-1);
	else
		stop_core_all(-1);

	cur_tsc = rte_rdtsc();
	if (prox_cfg.duration_time != 0) {
		stop_tsc = cur_tsc + sec_to_tsc(prox_cfg.start_time + prox_cfg.duration_time);
	}

	stats_cons_notify();
	stats_cons_refresh();

	update_interval = str_to_tsc(prox_cfg.update_interval_str);
	next_update = cur_tsc + update_interval;

	while (stop_prox == 0) {

		if (update_interval < update_interval_threshold)
			busy_wait_until(next_update);
		else
			multiplexed_input_stats(next_update);

		next_update += update_interval;

		stats_update(stats_cons_flags);
		stats_cons_notify();

		if (stop_tsc && rte_rdtsc() >= stop_tsc) {
			stop_prox = 1;
		}
	}

	stats_cons_finish();

	if (prox_cfg.flags & DSF_WAIT_ON_QUIT) {
		stop_core_all(-1);
	}

	if (prox_cfg.logbuf) {
		file_print(prox_cfg.logbuf);
	}

	exit(EXIT_SUCCESS);
}
