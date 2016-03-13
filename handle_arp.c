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

#include "task_init.h"
#include "task_base.h"
#include "stats.h"
#include "arp.h"
#include "etypes.h"
#include "quit.h"
#include "log.h"
#include "prox_port_cfg.h"
#include "lconf.h"

struct task_arp {
	struct task_base   base;
	struct ether_addr  src_mac;
	uint32_t           seed;
};

static inline void prepare_arp_reply(struct task_arp *task, struct ether_hdr_arp *packet, struct ether_addr s_addr)
{
	uint32_t ip_source = packet->arp.data.spa;

	packet->arp.data.spa = packet->arp.data.tpa;
	packet->arp.data.tpa = ip_source;
	packet->arp.oper = 0x200;
	memcpy(&packet->arp.data.tha, &packet->arp.data.sha, sizeof(struct ether_addr));
	memcpy(&packet->arp.data.sha, &s_addr, sizeof(struct ether_addr));
}

static void create_mac(struct task_arp *task, struct ether_addr *addr)
{
	uint32_t rand = rand_r(&task->seed);

	addr->addr_bytes[0] = 0x2;
	addr->addr_bytes[1] = task->base.tx_params_hw_sw.tx_port_queue.port;
	memcpy(addr->addr_bytes + 2, &rand, 4);
}

static void handle_arp(struct task_arp *task, struct ether_hdr_arp *hdr)
{
	struct ether_addr s_addr;

	create_mac(task, &s_addr);
	prepare_arp_reply(task, hdr, s_addr);

	memcpy(hdr->ether_hdr.d_addr.addr_bytes, hdr->ether_hdr.s_addr.addr_bytes, 6);
	memcpy(hdr->ether_hdr.s_addr.addr_bytes, &s_addr, 6);
}

static void handle_arp_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct ether_hdr_arp *hdr;
	struct task_arp *task = (struct task_arp *)tbase;
	uint8_t out[MAX_PKT_BURST] = {0};
	struct rte_mbuf *arp_mbufs[64] = {0};
	int n_arp_pkts = 0, n_other_pkts = 0;

	for (uint16_t j = 0; j < n_pkts; ++j) {
		hdr = rte_pktmbuf_mtod(mbufs[j], struct ether_hdr_arp *);
		if (hdr->ether_hdr.ether_type == ETYPE_ARP) {
			if (arp_is_gratuitous(hdr)) {
				out[n_other_pkts] = OUT_DISCARD;
				n_other_pkts++;
				plog_info("Received gratuitous packet \n");
			} else {
				handle_arp(task, hdr);
				arp_mbufs[n_arp_pkts] = mbufs[j];
				n_arp_pkts++;
			}
		} else {
			mbufs[n_other_pkts] = mbufs[j];
			n_other_pkts++;
		}
	}
	if (n_arp_pkts)
		task->base.aux->tx_pkt_hw(&task->base, arp_mbufs, n_arp_pkts, out);
	task->base.tx_pkt(&task->base, mbufs, n_other_pkts, out);
}

static void init_task_arp(struct task_base *tbase, struct task_args *targ)
{
	struct task_arp *task = (struct task_arp *)tbase;

	task->seed = rte_rdtsc();
	PROX_PANIC(targ->nb_txports == 0, "arp mode must have a tx_port");
	memcpy(&task->src_mac, &prox_port_cfg[task->base.tx_params_hw_sw.tx_port_queue.port].eth_addr, sizeof(struct ether_addr));
}

static struct task_init task_init_arp = {
	.mode_str = "arp",
	.init = init_task_arp,
	.handle = handle_arp_bulk,
	.size = sizeof(struct task_arp)
};

__attribute__((constructor)) static void reg_task_arp(void)
{
	reg_task(&task_init_arp);
}
