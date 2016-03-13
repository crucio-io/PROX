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

#include <string.h>
#include <rte_table_hash.h>
#include <rte_version.h>
#include <rte_malloc.h>

#include "prox_malloc.h"
#include "display.h"
#include "commands.h"
#include "log.h"
#include "run.h"
#include "lconf.h"
#include "hash_utils.h"
#include "prox_cfg.h"
#include "prox_port_cfg.h"
#include "defines.h"
#include "handle_qos.h"
#include "handle_qinq_encap4.h"
#include "quit.h"
#include "input.h"
#include "rw_reg.h"

void start_core_all(int task_id)
{
	uint32_t cores[RTE_MAX_LCORE];
	uint32_t lcore_id;
	char tmp[256];
	int cnt = 0;

	prox_core_to_str(tmp, sizeof(tmp), 0);
	plog_info("Starting cores: %s\n", tmp);

	lcore_id = -1;
	while (prox_core_next(&lcore_id, 0) == 0) {
		cores[cnt++] = lcore_id;
	}
	start_cores(cores, cnt, task_id);
}

void stop_core_all(int task_id)
{
	uint32_t cores[RTE_MAX_LCORE];
	uint32_t lcore_id;
	char tmp[256];
	int cnt = 0;

	prox_core_to_str(tmp, sizeof(tmp), 0);
	plog_info("Stopping cores: %s\n", tmp);

	lcore_id = -1;
	while (prox_core_next(&lcore_id, 0) == 0) {
		cores[cnt++] = lcore_id;
	}

	stop_cores(cores, cnt, task_id);
}

static void warn_inactive_cores(uint32_t *cores, int count, const char *prefix)
{
	for (int i = 0; i < count; ++i) {
		if (!prox_core_active(cores[i], 0)) {
			plog_warn("%s %u: core is not active\n", prefix, cores[i]);
		}
	}
}

void start_cores(uint32_t *cores, int count, int task_id)
{
	int n_started_cores = 0;
	uint32_t started_cores[RTE_MAX_LCORE];

	warn_inactive_cores(cores, count, "Can't start core");

	for (int i = 0; i < count; ++i) {
		struct lcore_cfg *lconf = &lcore_cfg[cores[i]];

		if (lconf->n_tasks_run != lconf->n_tasks_all) {

			lconf->msg.type = LCONF_MSG_START;
			lconf->msg.task_id = task_id;
			lconf_set_req(lconf);
			if (task_id == -1)
				plog_info("Starting core %u (all tasks)\n", cores[i]);
			else
				plog_info("Starting core %u task %u\n", cores[i], task_id);
			started_cores[n_started_cores++] = cores[i];
			lconf->flags |= LCONF_FLAG_RUNNING;
			rte_eal_remote_launch(lconf_run, NULL, cores[i]);
		}
		else {
			plog_warn("Core %u is already running all its tasks\n", cores[i]);
		}
	}

	/* This function is blocking, so detect when each core has
	   consumed the message. */
	for (int i = 0; i < n_started_cores; ++i) {
		struct lcore_cfg *lconf = &lcore_cfg[started_cores[i]];
		plog_info("Waiting for core %u to start...", started_cores[i]);
		while (lconf_is_req(lconf)) ;
		plog_info(" OK\n");
	}
}

void stop_cores(uint32_t *cores, int count, int task_id)
{
	int n_stopped_cores = 0;
	uint32_t stopped_cores[RTE_MAX_LCORE];
	uint32_t c;

	warn_inactive_cores(cores, count, "Can't stop core");

	for (int i = 0; i < count; ++i) {
		struct lcore_cfg *lconf = &lcore_cfg[cores[i]];
		if (lconf->n_tasks_run) {
			while (lconf_is_req(lconf));

			lconf->msg.type = LCONF_MSG_STOP;
			lconf->msg.task_id = task_id;
			lconf_set_req(lconf);
			stopped_cores[n_stopped_cores++] = cores[i];
		}
	}

	for (int i = 0; i < n_stopped_cores; ++i) {
		c = stopped_cores[i];
		struct lcore_cfg *lconf = &lcore_cfg[c];
		while (lconf_is_req(lconf));

		if (lconf->n_tasks_run == 0) {
			plog_info("All tasks stopped on core %u, waiting for core to stop...", c);
			rte_eal_wait_lcore(c);
			plog_info(" OK\n");
			lconf->flags &= ~LCONF_FLAG_RUNNING;
		}
		else {
			plog_info("Stopped task %u on core %u\n", task_id, c);
		}
	}
}

struct size_unit {
	uint64_t val;
	uint64_t frac;
	char     unit[8];
};

static struct size_unit to_size_unit(uint64_t bytes)
{
	struct size_unit ret;

	if (bytes > 1 << 30) {
		ret.val = bytes >> 30;
		ret.frac = ((bytes - (ret.val << 30)) * 1000) / (1 << 30);
		strcpy(ret.unit, "GB");
	}
	else if (bytes > 1 << 20) {
		ret.val = bytes >> 20;
		ret.frac = ((bytes - (ret.val << 20)) * 1000) / (1 << 20);
		strcpy(ret.unit, "MB");
	}
	else if (bytes > 1 << 10) {
		ret.val = bytes >> 10;
		ret.frac = (bytes - (ret.val << 10)) * 1000 / (1 << 10);
		strcpy(ret.unit, "KB");
	}
	else {
		ret.val = bytes;
		ret.frac = 0;
		strcpy(ret.unit, "B");
	}

	return ret;
}

void cmd_mem_stats(void)
{
	struct rte_malloc_socket_stats sock_stats;
	uint64_t v;
	struct size_unit su;

	for (uint32_t i = 0; i < RTE_MAX_NUMA_NODES; ++i) {
		if (rte_malloc_get_socket_stats(i, &sock_stats) < 0 || sock_stats.heap_totalsz_bytes == 0)
			continue;

		plogx_info("Socket %u memory stats:\n", i);
		su = to_size_unit(sock_stats.heap_totalsz_bytes);
		plogx_info("\tHeap_size: %zu.%03zu %s\n", su.val, su.frac, su.unit);
		su = to_size_unit(sock_stats.heap_freesz_bytes);
		plogx_info("\tFree_size: %zu.%03zu %s\n", su.val, su.frac, su.unit);
		su = to_size_unit(sock_stats.heap_allocsz_bytes);
		plogx_info("\tAlloc_size: %zu.%03zu %s\n", su.val, su.frac, su.unit);
		su = to_size_unit(sock_stats.greatest_free_size);
		plogx_info("\tGreatest_free_size: %zu %s\n", su.val, su.unit);
		plogx_info("\tAlloc_count: %u\n", sock_stats.alloc_count);
		plogx_info("\tFree_count: %u\n", sock_stats.free_count);
	}
}

void cmd_mem_layout(void)
{
	const struct rte_memseg* memseg = rte_eal_get_physmem_layout();

	plog_info("Memory layout:\n");
	for (uint32_t i = 0; i < RTE_MAX_MEMSEG; i++) {
		if (memseg[i].addr == NULL)
			break;

		const char *sz_str;
		switch (memseg[i].hugepage_sz >> 20) {
		case 2:
			sz_str = "2MB";
			break;
		case 1024:
			sz_str = "1GB";
			break;
		default:
			sz_str = "??";
		}

		plog_info("Segment %u: [%#lx-%#lx] at %p using %zu pages of %s\n",
			  i,
			  memseg[i].phys_addr,
			  memseg[i].phys_addr + memseg[i].len,
			  memseg[i].addr,
			  memseg[i].len/memseg[i].hugepage_sz, sz_str);
	}
}

void cmd_dump(uint8_t lcore_id, uint8_t task_id, uint32_t nb_packets, struct input *input, int rx, int tx)
{
	plog_info("dump %u %u %u\n", lcore_id, task_id, nb_packets);
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	}
	else if (task_id >= lcore_cfg[lcore_id].n_tasks_all) {
		plog_warn("task_id too high, should be in [0, %u]\n", lcore_cfg[lcore_id].n_tasks_all - 1);
	}
	else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		lconf->tasks_all[task_id]->aux->task_rt_dump.input = input;

		while (lconf_is_req(lconf));
		if (rx && tx)
			lconf->msg.type = LCONF_MSG_DUMP;
		else if (rx)
			lconf->msg.type = LCONF_MSG_DUMP_RX;
		else if (tx)
			lconf->msg.type = LCONF_MSG_DUMP_TX;

		if (rx || tx) {
			lconf->msg.task_id = task_id;
			lconf->msg.val  = nb_packets;
			lconf_set_req(lconf);
		}

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_trace(uint8_t lcore_id, uint8_t task_id, uint32_t nb_packets)
{
	plog_info("trace %u %u %u\n", lcore_id, task_id, nb_packets);
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	}
	else if (task_id >= lcore_cfg[lcore_id].n_tasks_all) {
		plog_warn("task_id too high, should be in [0, %u]\n", lcore_cfg[lcore_id].n_tasks_all - 1);
	}
	else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));

		lconf->msg.type = LCONF_MSG_TRACE;
		lconf->msg.task_id = task_id;
		lconf->msg.val  = nb_packets;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_rx_bw_start(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if (lcore_cfg[lcore_id].flags & LCONF_FLAG_RX_BW_ACTIVE) {
		plog_warn("rx bandwidt already on core %u\n", lcore_id);
	} else {

		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_RX_BW_START;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_tx_bw_start(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if (lcore_cfg[lcore_id].flags & LCONF_FLAG_TX_BW_ACTIVE) {
		plog_warn("tx bandwidth already running on core %u\n", lcore_id);
	} else {

		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_TX_BW_START;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_rx_bw_stop(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if (!(lcore_cfg[lcore_id].flags & LCONF_FLAG_RX_BW_ACTIVE)) {
		plog_warn("rx bandwidth not running on core %u\n", lcore_id);
	} else {

		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_RX_BW_STOP;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_tx_bw_stop(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if (!(lcore_cfg[lcore_id].flags & LCONF_FLAG_TX_BW_ACTIVE)) {
		plog_warn("tx bandwidth not running on core %u\n", lcore_id);
	} else {

		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_TX_BW_STOP;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}
void cmd_rx_distr_start(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if (lcore_cfg[lcore_id].flags & LCONF_FLAG_RX_DISTR_ACTIVE) {
		plog_warn("rx distribution already xrunning on core %u\n", lcore_id);
	} else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_RX_DISTR_START;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_tx_distr_start(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if (lcore_cfg[lcore_id].flags & LCONF_FLAG_TX_DISTR_ACTIVE) {
		plog_warn("tx distribution already xrunning on core %u\n", lcore_id);
	} else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_TX_DISTR_START;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_rx_distr_stop(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if ((lcore_cfg[lcore_id].flags & LCONF_FLAG_RX_DISTR_ACTIVE) == 0) {
		plog_warn("rx distribution not running on core %u\n", lcore_id);
	} else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_RX_DISTR_STOP;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_tx_distr_stop(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else if ((lcore_cfg[lcore_id].flags & LCONF_FLAG_TX_DISTR_ACTIVE) == 0) {
		plog_warn("tx distribution not running on core %u\n", lcore_id);
	} else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_TX_DISTR_STOP;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_rx_distr_rst(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_RX_DISTR_RESET;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_tx_distr_rst(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else {
		struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

		while (lconf_is_req(lconf));
		lconf->msg.type = LCONF_MSG_TX_DISTR_RESET;
		lconf_set_req(lconf);

		if (lconf->n_tasks_run == 0) {
			lconf_do_flags(lconf);
		}
	}
}

void cmd_rx_distr_show(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else {
		for (uint32_t i = 0; i < lcore_cfg[lcore_id].n_tasks_all; ++i) {
			struct task_base *t = lcore_cfg[lcore_id].tasks_all[i];
			plog_info("t[%u]: ", i);
			for (uint32_t j = 0; j < sizeof(t->aux->rx_bucket)/sizeof(t->aux->rx_bucket[0]); ++j) {
				plog_info("%u ", t->aux->rx_bucket[j]);
			}
			plog_info("\n");
		}
	}
}
void cmd_tx_distr_show(uint32_t lcore_id)
{
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	} else {
		for (uint32_t i = 0; i < lcore_cfg[lcore_id].n_tasks_all; ++i) {
			struct task_base *t = lcore_cfg[lcore_id].tasks_all[i];
			uint64_t tot = 0, avg = 0;
			for (uint32_t j = 0; j < sizeof(t->aux->tx_bucket)/sizeof(t->aux->tx_bucket[0]); ++j) {
				tot += t->aux->tx_bucket[j];
				avg += j * t->aux->tx_bucket[j];
			}
			avg = avg / tot;
			plog_info("t[%u]: %lu: ", i, avg);
			for (uint32_t j = 0; j < sizeof(t->aux->tx_bucket)/sizeof(t->aux->tx_bucket[0]); ++j) {
				plog_info("%u ", t->aux->tx_bucket[j]);
			}
			plog_info("\n");
		}
	}
}

void cmd_ringinfo_all(void)
{
	struct lcore_cfg *lconf;
	uint32_t lcore_id = -1;

	while(prox_core_next(&lcore_id, 0) == 0) {
		lconf = &lcore_cfg[lcore_id];
		for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
			cmd_ringinfo(lcore_id, task_id);
		}
	}
}

void cmd_ringinfo(uint8_t lcore_id, uint8_t task_id)
{
	struct lcore_cfg *lconf;
	struct rte_ring *ring;
	struct task_args* targ;
	uint32_t count;

	if (!prox_core_active(lcore_id, 0)) {
		plog_info("lcore %u is not active\n", lcore_id);
		return;
	}
	lconf = &lcore_cfg[lcore_id];
	if (task_id >= lconf->n_tasks_all) {
		plog_warn("Invalid task index %u: lcore %u has %u tasks\n", task_id, lcore_id, lconf->n_tasks_all);
		return;
	}

	targ = &lconf->targs[task_id];
	plog_info("Core %u task %u: %u rings\n", lcore_id, task_id, targ->nb_rxrings);
	for (uint8_t i = 0; i < targ->nb_rxrings; ++i) {
		ring = targ->rx_rings[i];
		count = ring->prod.mask + 1;
		plog_info("\tRing %u:\n", i);
		plog_info("\t\tFlags: %s,%s\n", ring->flags & RING_F_SP_ENQ? "sp":"mp", ring->flags & RING_F_SC_DEQ? "sc":"mc");
		plog_info("\t\tMemory size: %zu bytes\n", rte_ring_get_memsize(count));
		plog_info("\t\tOccupied: %u/%u\n", rte_ring_count(ring), count);
	}
}

void cmd_port_up(uint8_t port_id)
{
	int err;

	if (!port_is_active(port_id)) {
		return ;
	}

	if ((err = rte_eth_dev_set_link_up(port_id)) == 0) {
		plog_info("Bringing port %d up\n", port_id);
	}
	else {
		plog_warn("Failed to bring port %d up with error %d\n", port_id, err);
	}
}

void cmd_port_down(uint8_t port_id)
{
	int err;

	if (!port_is_active(port_id)) {
		return ;
	}

	if ((err = rte_eth_dev_set_link_down(port_id)) == 0) {
		plog_info("Bringing port %d down\n", port_id);
	}
	else {
		plog_warn("Failed to bring port %d down with error %d\n", port_id, err);
	}
}

void cmd_xstats(uint8_t port_id)
{
#if RTE_VERSION >= RTE_VERSION_NUM(2,1,0,0)
	int n_xstats;
	struct rte_eth_xstats *eth_xstats;
	struct prox_port_cfg* port_cfg = &prox_port_cfg[port_id];
	int rc;

	n_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	eth_xstats = prox_zmalloc(n_xstats * sizeof(*eth_xstats), port_cfg->socket);
	PROX_ASSERT(eth_xstats);
	rc = rte_eth_xstats_get(port_id, eth_xstats, n_xstats);
	if ((rc < 0) || (rc > n_xstats)) {
		if (rc < 0) {
			plog_warn("Failed to get xstats on port %d with error %d\n", port_id, rc);
		} else if (rc > n_xstats) {
			plog_warn("Failed to get xstats on port %d: too many xstats (%d)\n", port_id, rc);
		}
	} else {
		for (int i=0;i<rc;i++) {
			plog_info("%s: %ld\n", eth_xstats[i].name, eth_xstats[i].value);
		}
	}
#else
	plog_warn("Failed to get xstats, xstats are not supported in this version of dpdk\n");
#endif
}

void cmd_portinfo(int port_id, char *dst, size_t max_len)
{
	char *end = dst + max_len;

	*dst = 0;
	if (port_id == -1) {
		uint8_t max_port_idx = prox_last_port_active() + 1;

		for (uint8_t port_id = 0; port_id < max_port_idx; ++port_id) {
			if (!prox_port_cfg[port_id].active) {
				continue;
			}
			struct prox_port_cfg* port_cfg = &prox_port_cfg[port_id];

			dst += snprintf(dst, end - dst,
					"%2d:%10s; "MAC_BYTES_FMT"; %s\n",
					port_id,
					port_cfg->name,
					MAC_BYTES(port_cfg->eth_addr.addr_bytes),
					port_cfg->pci_addr);
		}
		return;
	}

	if (!port_is_active(port_id)) {
		return ;
	}

	struct prox_port_cfg* port_cfg = &prox_port_cfg[port_id];

	dst += snprintf(dst, end - dst, "Port info for port %u\n", port_id);
	dst += snprintf(dst, end - dst, "\tName: %s\n", port_cfg->name);
	dst += snprintf(dst, end - dst, "\tDriver: %s\n", port_cfg->driver_name);
	dst += snprintf(dst, end - dst, "\tMac address: "MAC_BYTES_FMT"\n", MAC_BYTES(port_cfg->eth_addr.addr_bytes));
	dst += snprintf(dst, end - dst, "\tLink speed: %u Mbps\n", port_cfg->link_speed);
	dst += snprintf(dst, end - dst, "\tLink status: %s\n", port_cfg->link_up? "up" : "down");
	dst += snprintf(dst, end - dst, "\tSocket: %u\n", port_cfg->socket);
	dst += snprintf(dst, end - dst, "\tPCI address: %s\n", port_cfg->pci_addr);
	dst += snprintf(dst, end - dst, "\tPromiscuous: %s\n", port_cfg->promiscuous? "yes" : "no");
	dst += snprintf(dst, end - dst, "\tNumber of RX/TX descriptors: %u/%u\n", port_cfg->n_rxd, port_cfg->n_txd);
	dst += snprintf(dst, end - dst, "\tNumber of RX/TX queues: %u/%u (max: %u/%u)\n", port_cfg->n_rxq, port_cfg->n_txq, port_cfg->max_rxq, port_cfg->max_txq);
	dst += snprintf(dst, end - dst, "\tMemory pools:\n");

	for (uint8_t i = 0; i < 32; ++i) {
		if (port_cfg->pool[i]) {
			dst += snprintf(dst, end - dst, "\t\tname: %s (%p)\n",
					port_cfg->pool[i]->name, port_cfg->pool[i]);
		}
	}
}

void cmd_read_reg(uint8_t port_id, unsigned int id)
{
	unsigned int val, rc;
	if (!port_is_active(port_id)) {
		return ;
	}
	rc = read_reg(port_id, id, &val);
	if (rc) {
		plog_warn("Failed to read register %d on port %d\n", id, port_id);
	}
	else {
		plog_info("Register 0x%08X : %08X \n", id, val);
	}
}

void cmd_reset_port(uint8_t portid)
{
	unsigned int rc;
	if (!prox_port_cfg[portid].active) {
		plog_info("port not active \n");
		return;
	}
	rte_eth_dev_stop(portid);
	rc = rte_eth_dev_start(portid);
	if (rc) {
		plog_warn("Failed to restart port %d\n", portid);
	}
}
void cmd_write_reg(uint8_t port_id, unsigned int id, unsigned int val)
{
	if (!port_is_active(port_id)) {
		return ;
	}

	plog_info("writing 0x%08X %08X\n", id, val);
	write_reg(port_id, id, val);
}

void cmd_set_vlan_offload(uint8_t port_id, unsigned int val)
{
	if (!port_is_active(port_id)) {
		return ;
	}

	plog_info("setting vlan offload to %d\n", val);
	if (val & ~(ETH_VLAN_STRIP_OFFLOAD | ETH_VLAN_FILTER_OFFLOAD | ETH_VLAN_EXTEND_OFFLOAD)) {
		plog_info("wrong vlan offload value\n");
	}
	int ret = rte_eth_dev_set_vlan_offload(port_id, val);
	plog_info("rte_eth_dev_set_vlan_offload return %d\n", ret);
}

void cmd_set_vlan_filter(uint8_t port_id, unsigned int id, unsigned int val)
{
	if (!port_is_active(port_id)) {
		return ;
	}

	plog_info("setting vln filter for vlan %d to %d\n", id, val);
	int ret = rte_eth_dev_vlan_filter(port_id, id, val);
	plog_info("rte_eth_dev_vlan_filter return %d\n", ret);
}

void cmd_thread_info(uint8_t lcore_id, uint8_t task_id)
{
	plog_info("thread_info %u %u \n", lcore_id, task_id);
	if (lcore_id > RTE_MAX_LCORE) {
		plog_warn("core_id too high, maximum allowed is: %u\n", RTE_MAX_LCORE);
	}
	if (!prox_core_active(lcore_id, 0)) {
		plog_warn("lcore %u is not active\n", lcore_id);
		return;
	}
	if (task_id >= lcore_cfg[lcore_id].n_tasks_all) {
		plog_warn("task_id too high, should be in [0, %u]\n", lcore_cfg[lcore_id].n_tasks_all - 1);
		return;
	}
	if (strcmp(lcore_cfg[lcore_id].targs[task_id].task_init->mode_str, "qos") == 0) {
		struct task_base *task;

		task = lcore_cfg[lcore_id].tasks_all[task_id];
		plog_info("core %d, task %d: %d mbufs stored in QoS\n", lcore_id, task_id,
			  task_qos_n_pkts_buffered(task));

#ifdef ENABLE_EXTRA_USER_STATISTICS
	}
	else if (lcore_cfg[lcore_id].targs[task_id].mode == QINQ_ENCAP4) {
		struct task_qinq_encap4 *task;
		task = (struct task_qinq_encap4 *)(lcore_cfg[lcore_id].tasks_all[task_id]);
		for (int i=0;i<task->n_users;i++) {
			if (task->stats_per_user[i])
				plog_info("User %d: %d packets\n", i, task->stats_per_user[i]);
		}
#endif
	}
	else {
		// Only QoS thread info so far
		plog_err("core %d, task %d: not a qos core (%p)\n", lcore_id, task_id, lcore_cfg[lcore_id].thread_x);
	}
}

void cmd_rx_tx_info(void)
{
	uint32_t lcore_id = -1;
	while(prox_core_next(&lcore_id, 0) == 0) {
		for (uint8_t task_id = 0; task_id < lcore_cfg[lcore_id].n_tasks_all; ++task_id) {
			struct task_args *targ = &lcore_cfg[lcore_id].targs[task_id];

			plog_info("Core %u:", lcore_id);
			if (targ->rx_ports[0] != OUT_DISCARD) {
				for (int i = 0; i < targ->nb_rxports; i++) {
					plog_info(" RX port %u (queue %u)", targ->rx_ports[i], targ->rx_queues[i]);
				}
			}
			else {
				for (uint8_t j = 0; j < targ->nb_rxrings; ++j) {
					plog_info(" RX ring[%u,%u] %p", task_id, j, targ->rx_rings[j]);
				}
			}
			plog_info(" ==>");
			for (uint8_t j = 0; j < targ->nb_txports; ++j) {
				plog_info(" TX port %u (queue %u)", targ->tx_port_queue[j].port,
					  targ->tx_port_queue[j].queue);
			}

			for (uint8_t j = 0; j < targ->nb_txrings; ++j) {
				plog_info(" TX ring %p", targ->tx_rings[j]);
			}

			plog_info("\n");
		}
	}
}
