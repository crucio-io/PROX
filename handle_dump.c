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

#include <rte_cycles.h>
#include <pcap.h>

#include "prox_malloc.h"
#include "clock.h"
#include "log.h"
#include "lconf.h"
#include "task_init.h"
#include "task_base.h"
#include "stats.h"

struct task_dump {
	struct task_base base;
	uint32_t n_mbufs;
        struct rte_mbuf **mbufs;
	uint32_t n_pkts;
	char pcap_file[128];
};

static uint16_t buffer_packets(struct task_dump *task, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	uint16_t j = 0;

	if (task->n_mbufs == task->n_pkts)
		return 0;

	for (j = 0; j < n_pkts && task->n_mbufs < task->n_pkts; ++j) {
		mbufs[j]->udata64 = rte_rdtsc();
		task->mbufs[task->n_mbufs++] = mbufs[j];
	}

	return j;
}

static int handle_dump_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_dump *task = (struct task_dump *)tbase;
	const uint16_t ofs = buffer_packets(task, mbufs, n_pkts);

	for (uint16_t j = ofs; j < n_pkts; ++j)
		rte_pktmbuf_free(mbufs[j]);
	TASK_STATS_ADD_DROP_DISCARD(&tbase->aux->stats, n_pkts - ofs);
	return n_pkts;
}

static void init_task_dump(struct task_base *tbase, __attribute__((unused)) struct task_args *targ)
{
	struct task_dump *task = (struct task_dump *)tbase;
	const int socket_id = rte_lcore_to_socket_id(targ->lconf->id);

	task->mbufs = prox_zmalloc(sizeof(*task->mbufs) * targ->n_pkts, socket_id);
	task->n_pkts = targ->n_pkts;
	if (!strcmp(targ->pcap_file, "")) {
		strcpy(targ->pcap_file, "out.pcap");
	}
	strncpy(task->pcap_file, targ->pcap_file, sizeof(task->pcap_file));
}

static void stop(struct task_base *tbase)
{
	struct task_dump *task = (struct task_dump *)tbase;
	static pcap_dumper_t *pcap_dump_handle;
	pcap_t *handle;
	uint32_t n_pkts = 65536;
	struct pcap_pkthdr header = {{0}, 0, 0};
	static int once = 0;
	char err_str[PCAP_ERRBUF_SIZE];
	const uint64_t hz = rte_get_tsc_hz();
	struct timeval tv = {0};
	uint64_t tsc, beg = 0;

	plogx_info("Dumping %d packets to '%s'\n", task->n_mbufs, task->pcap_file);
	handle = pcap_open_dead(DLT_EN10MB, n_pkts);
	pcap_dump_handle = pcap_dump_open(handle, task->pcap_file);

	if (task->n_mbufs) {
		beg = task->mbufs[0]->udata64;
	}
	for (uint32_t j = 0; j < task->n_mbufs; ++j) {
		tsc = task->mbufs[j]->udata64 - beg;
		header.len = rte_pktmbuf_pkt_len(task->mbufs[j]);
		header.caplen = header.len;
		tsc_to_tv(&header.ts, tsc);
		pcap_dump((unsigned char *)pcap_dump_handle, &header, rte_pktmbuf_mtod(task->mbufs[j], void *));
	}

	pcap_dump_close(pcap_dump_handle);
	pcap_close(handle);
	plogx_info("Dump complete, releasing mbufs\n");

	uint32_t j = 0;

	while (j + 64 < task->n_mbufs) {
		tbase->tx_pkt(tbase, &task->mbufs[j], 64, NULL);
		j += 64;
	}
	if (j < task->n_mbufs) {
		tbase->tx_pkt(tbase, &task->mbufs[j], task->n_mbufs - j, NULL);
	}
	task->n_mbufs = 0;
}

static struct task_init task_init_dump = {
	.mode_str = "dump",
	.init = init_task_dump,
	.handle = handle_dump_bulk,
	.stop = stop,
	.flag_features = TASK_FEATURE_ZERO_RX,
	.size = sizeof(struct task_dump)
};

__attribute__((constructor)) static void reg_task_dump(void)
{
	reg_task(&task_init_dump);
}
