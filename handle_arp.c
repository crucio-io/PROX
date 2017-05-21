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

#include "task_init.h"
#include "task_base.h"
#include "stats.h"
#include "arp.h"
#include "etypes.h"
#include "quit.h"
#include "log.h"
#include "prox_port_cfg.h"
#include "lconf.h"
#include "cmd_parser.h"
#include "handle_arp.h"

#define CPE_ARP		0x1

struct task_arp {
	struct task_base   base;
	struct ether_addr  src_mac;
	uint32_t           seed;
	uint32_t           flags;
	uint32_t           ip;
	uint32_t           tmp_ip;
	uint8_t	           arp_replies_ring;
	uint8_t            other_pkts_ring;
	uint8_t            sending_replies_ring;
};

static void task_update_config(struct task_arp *task)
{
	if (unlikely(task->ip != task->tmp_ip))
		task->ip = task->tmp_ip;
}

static inline void prepare_arp_reply(struct task_arp *task, struct ether_hdr_arp *packet, struct ether_addr *s_addr)
{
	uint32_t ip_source = packet->arp.data.spa;

	packet->arp.data.spa = packet->arp.data.tpa;
	packet->arp.data.tpa = ip_source;
	packet->arp.oper = 0x200;
	memcpy(&packet->arp.data.tha, &packet->arp.data.sha, sizeof(struct ether_addr));
	memcpy(&packet->arp.data.sha, s_addr, sizeof(struct ether_addr));
}

static void create_mac(struct task_arp *task, struct ether_hdr_arp *hdr, struct ether_addr *addr)
{
	addr->addr_bytes[0] = 0x2;
	addr->addr_bytes[1] = task->base.tx_params_hw_sw.tx_port_queue.port;
	// Instead of sending a completely random MAC address, create the following MAC:
	// 02:x0:x1:x2:x3:x4 where x0 is a port number and x1:x2:x3:x4 is the IP address
	memcpy(addr->addr_bytes + 2, (uint32_t *)&hdr->arp.data.tpa, 4);
}

static void handle_arp(struct task_arp *task, struct ether_hdr_arp *hdr, struct ether_addr *s_addr)
{
	prepare_arp_reply(task, hdr, s_addr);
	memcpy(hdr->ether_hdr.d_addr.addr_bytes, hdr->ether_hdr.s_addr.addr_bytes, 6);
	memcpy(hdr->ether_hdr.s_addr.addr_bytes, s_addr, 6);
}

static int handle_arp_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct ether_hdr_arp *hdr;
	struct task_arp *task = (struct task_arp *)tbase;
	uint8_t out[MAX_PKT_BURST] = {0};
	struct rte_mbuf *arp_mbufs[64] = {0}, *replies_mbufs[64] = {0};
	int n_arp_pkts = 0, n_other_pkts = 0,n_replies_pkts = 0;
	struct ether_addr s_addr;

	for (uint16_t j = 0; j < n_pkts; ++j) {
		hdr = rte_pktmbuf_mtod(mbufs[j], struct ether_hdr_arp *);
		if (hdr->ether_hdr.ether_type == ETYPE_ARP) {
			if (arp_is_gratuitous(hdr)) {
				out[n_other_pkts] = OUT_DISCARD;
				n_other_pkts++;
				plog_info("Received gratuitous packet \n");
			} else if (hdr->arp.oper == 0x100) {
				if (task->flags & CPE_ARP) {
					create_mac(task, hdr, &s_addr);
					handle_arp(task, hdr, &s_addr);
					arp_mbufs[n_arp_pkts] = mbufs[j];
					if (task->sending_replies_ring != OUT_DISCARD)
						out[n_arp_pkts] = task->sending_replies_ring;
					else
						out[n_arp_pkts] = 0;
					n_arp_pkts++;
				} else if (hdr->arp.data.tpa == task->ip) {
					handle_arp(task, hdr, &task->src_mac);
					arp_mbufs[n_arp_pkts] = mbufs[j];
					if (task->sending_replies_ring != OUT_DISCARD)
						out[n_arp_pkts] = task->sending_replies_ring;
					else
						out[n_arp_pkts] = 0;
					n_arp_pkts++;
				} else {
					out[n_other_pkts] = OUT_DISCARD;
					mbufs[n_other_pkts] = mbufs[j];
					n_other_pkts++;
					plog_info("Received ARP on unexpected IP %x, expecting %x\n", rte_be_to_cpu_32(hdr->arp.data.tpa), rte_be_to_cpu_32(task->ip));
				}
			} else if (hdr->arp.oper == 0x200) {
				replies_mbufs[n_replies_pkts] = mbufs[j];
				out[n_replies_pkts] = task->arp_replies_ring;
				n_replies_pkts++;
			} else {
				out[n_other_pkts] = task->other_pkts_ring;
				mbufs[n_other_pkts] = mbufs[j];
				n_other_pkts++;
			}
		} else {
			out[n_other_pkts] = task->other_pkts_ring;
			mbufs[n_other_pkts] = mbufs[j];
			n_other_pkts++;
		}
	}
	int ret = 0;

	if (n_arp_pkts) {
		if (task->sending_replies_ring == OUT_DISCARD)
			ret+=task->base.aux->tx_pkt_hw(&task->base, arp_mbufs, n_arp_pkts, out);
		else
			ret+=task->base.tx_pkt(&task->base, arp_mbufs, n_arp_pkts, out);
	}
	if (n_replies_pkts)
		ret+= task->base.tx_pkt(&task->base, replies_mbufs, n_replies_pkts, out);
	ret+= task->base.tx_pkt(&task->base, mbufs, n_other_pkts, out);
	task_update_config(task);
	return ret;
}

void task_arp_set_local_ip(struct task_base *tbase, uint32_t ip)
{
	struct task_arp *task = (struct task_arp *)tbase;
	task->tmp_ip = ip;
}

static void init_task_arp(struct task_base *tbase, struct task_args *targ)
{
	struct task_arp *task = (struct task_arp *)tbase;
	struct task_args *dtarg;
	struct core_task ct;
	int port_found = 0;
	task->other_pkts_ring = OUT_DISCARD;
	task->arp_replies_ring = OUT_DISCARD;
	task->sending_replies_ring = OUT_DISCARD;

	task->seed = rte_rdtsc();
	memcpy(&task->src_mac, &prox_port_cfg[task->base.tx_params_hw_sw.tx_port_queue.port].eth_addr, sizeof(struct ether_addr));
	if (!strcmp(targ->task_init->sub_mode_str, "local")) {
		PROX_PANIC(targ->local_ipv4 == 0, "IP not configured: local arp sub_mode requires local_ipv4 parameter\n");
		task->ip = rte_cpu_to_be_32(targ->local_ipv4);
		task->tmp_ip = task->ip;
	} else
		task->flags |= CPE_ARP;

	PROX_PANIC(targ->nb_txrings > targ->core_task_set[0].n_elems, "%d txrings but %d elems in task_set\n", targ->nb_txrings, targ->core_task_set[0].n_elems);
	for (uint32_t i = 0; i < targ->nb_txrings; ++i) {
		ct = targ->core_task_set[0].core_task[i];
		plog_info("ARP mode checking whether core %d task %d (i.e. ring %d) can handle arp\n", ct.core, ct.task, i);
		if (task_is_mode(ct.core, ct.task, "gen", "l3")) {
			plog_info("ARP task sending ARP replies to core %d and task %d to handle them\n", ct.core, ct.task);
			task->arp_replies_ring = i;
		} else {
			dtarg = core_targ_get(ct.core, ct.task);
			if (((dtarg = find_reachable_task_sending_to_port(dtarg)) != NULL) &&
				(task_init_flag_set(dtarg->task_init, TASK_FEATURE_SENDING_ARP_REPLIES) == 1)) {
				plog_info("ARP task sending ARP replies to core %d and task %d to forward them\n", ct.core, ct.task);
				task->sending_replies_ring = i;
			} else {
				task->other_pkts_ring = i;
			}
		}
	}
	if ((targ->nb_txports == 0) && (task->sending_replies_ring == OUT_DISCARD)) {
		PROX_PANIC(1, "arp mode must have a tx_port or a ring able to reach a tx port");
	}
}

// Reply to ARP requests with random MAC addresses
static struct task_init task_init_cpe_arp = {
	.mode_str = "arp",
	.init = init_task_arp,
	.handle = handle_arp_bulk,
	.size = sizeof(struct task_arp)
};

// Reply to ARP requests with MAC address of the interface
static struct task_init task_init_arp = {
	.mode_str = "arp",
	.sub_mode_str = "local",
	.init = init_task_arp,
	.handle = handle_arp_bulk,
	.size = sizeof(struct task_arp)
};

__attribute__((constructor)) static void reg_task_arp(void)
{
	reg_task(&task_init_cpe_arp);
	reg_task(&task_init_arp);
}
