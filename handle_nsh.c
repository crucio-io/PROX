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

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "vxlangpe_nsh.h"
#include "task_base.h"
#include "tx_pkt.h"
#include "task_init.h"
#include "thread_generic.h"
#include "prefetch.h"
#include "log.h"

#define VXLAN_GPE_HDR_SZ sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + sizeof(struct vxlan_gpe_hdr) + sizeof(struct nsh_hdr)
#define ETHER_NSH_TYPE 0x4F89 /* 0x894F in little endian */
#define VXLAN_GPE_NSH_TYPE 0xB612 /* 4790 in little endian */
#define VXLAN_GPE_NP 0x4

uint16_t decap_nsh_packets(struct rte_mbuf **mbufs, uint16_t n_pkts);
uint16_t encap_nsh_packets(struct rte_mbuf **mbufs, uint16_t n_pkts);

struct task_decap_nsh {
	struct task_base base;
};

struct task_encap_nsh {
	struct task_base base;
};

static void init_task_decap_nsh(__attribute__((unused)) struct task_base *tbase,
			     __attribute__((unused)) struct task_args *targ)
{
	return;
}

static inline uint8_t handle_decap_nsh(__attribute__((unused)) struct task_decap_nsh *task, struct rte_mbuf *mbuf)
{
	struct ether_hdr *eth_hdr = NULL;
	struct udp_hdr *udp_hdr = NULL;
	struct vxlan_gpe_hdr *vxlan_gpe_hdr = NULL;
	uint16_t hdr_len;

	eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
	if (eth_hdr->ether_type == ETHER_NSH_TYPE) {
		/* "decapsulate" Ethernet + NSH header by moving packet pointer */
		hdr_len = sizeof(struct ether_hdr) + sizeof(struct nsh_hdr);

		mbuf->data_len = (uint16_t)(mbuf->data_len - hdr_len);
		mbuf->data_off += hdr_len;
		mbuf->pkt_len = (uint32_t)(mbuf->pkt_len - hdr_len);
		/* save length of header in reserved 16bits of rte_mbuf */
		mbuf->udata64 = hdr_len;
	}
	else {
		if (mbuf->data_len < VXLAN_GPE_HDR_SZ) {
			mbuf->udata64 = 0;
			return 0;
		}

		/* check the UDP destination port */
		udp_hdr = (struct udp_hdr *)(((unsigned char *)eth_hdr) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
		if (udp_hdr->dst_port != VXLAN_GPE_NSH_TYPE) {
			mbuf->udata64 = 0;
			return 0;
		}

		/* check the Next Protocol field in VxLAN-GPE header */
		vxlan_gpe_hdr = (struct vxlan_gpe_hdr *)(((unsigned char *)eth_hdr) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
		if (vxlan_gpe_hdr->next_proto != VXLAN_GPE_NP) {
			mbuf->udata64 = 0;
			return 0;
		}

		/* "decapsulate" VxLAN-GPE + NSH header by moving packet pointer */
		hdr_len = VXLAN_GPE_HDR_SZ;

		mbuf->data_len = (uint16_t)(mbuf->data_len - hdr_len);
		mbuf->data_off += hdr_len;
		mbuf->pkt_len  = (uint32_t)(mbuf->pkt_len - hdr_len);
		/* save length of header in reserved 16bits of rte_mbuf */
		mbuf->udata64 = hdr_len;
	}

	return 0;
}

static int handle_decap_nsh_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_decap_nsh *task = (struct task_decap_nsh *)tbase;
	uint8_t out[MAX_PKT_BURST];
	uint16_t j;

	prefetch_first(mbufs, n_pkts);
	for (j = 0; j + PREFETCH_OFFSET < n_pkts; ++j) {
#ifdef PROX_PREFETCH_OFFSET
		PREFETCH0(mbufs[j + PREFETCH_OFFSET]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PREFETCH_OFFSET - 1], void *));
#endif
		out[j] = handle_decap_nsh(task, mbufs[j]);
	}
#ifdef PROX_PREFETCH_OFFSET
	PREFETCH0(rte_pktmbuf_mtod(mbufs[n_pkts - 1], void *));
	for (; j < n_pkts; ++j) {
		out[j] = handle_decap_nsh(task, mbufs[j]);
	}
#endif
	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static void init_task_encap_nsh(__attribute__((unused)) struct task_base *tbase,
			      __attribute__((unused)) struct task_args *targ)
{
	return;
}

static inline uint8_t handle_encap_nsh(__attribute__((unused)) struct task_encap_nsh *task, struct rte_mbuf *mbuf)
{
	struct ether_hdr *eth_hdr = NULL;
	struct nsh_hdr *nsh_hdr = NULL;
	struct udp_hdr *udp_hdr = NULL;
	struct vxlan_gpe_hdr *vxlan_gpe_hdr = NULL;
	uint16_t hdr_len;

	if (mbuf == NULL)
		return 0;
	if (mbuf->udata64 == 0)
		return 0;

	/* use header length saved in reserved 16bits of rte_mbuf to
	   "encapsulate" transport + NSH header by moving packet pointer */
	mbuf->data_len = (uint16_t)(mbuf->data_len + mbuf->udata64);
	mbuf->data_off -= mbuf->udata64;
	mbuf->pkt_len  = (uint32_t)(mbuf->pkt_len + mbuf->udata64);

	eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
	if (eth_hdr->ether_type == ETHER_NSH_TYPE) {
		nsh_hdr = (struct nsh_hdr *) (((unsigned char *)eth_hdr) + sizeof(struct ether_hdr));

		/* decrement Service Index in NSH header */
		if (nsh_hdr->sf_index > 0)
			nsh_hdr->sf_index -= 1;
	}
	else {
		/* "encapsulate" VxLAN-GPE + NSH header by moving packet pointer */
		if (mbuf->data_len < VXLAN_GPE_HDR_SZ)
			return 0;

		/* check the UDP destination port */
		udp_hdr = (struct udp_hdr *)(((unsigned char *)eth_hdr) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
		if (udp_hdr->dst_port != VXLAN_GPE_NSH_TYPE)
			return 0;

		/* check the Next Protocol field in VxLAN-GPE header */
		vxlan_gpe_hdr = (struct vxlan_gpe_hdr *)(((unsigned char *)eth_hdr) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
		if (vxlan_gpe_hdr->next_proto != VXLAN_GPE_NP)
			return 0;

		/* decrement Service Index in NSH header */
		nsh_hdr = (struct nsh_hdr *)(((unsigned char *)vxlan_gpe_hdr) + sizeof(struct vxlan_gpe_hdr));
		if (nsh_hdr->sf_index > 0)
			nsh_hdr->sf_index -= 1;
	}

	return 0;
}

static int handle_encap_nsh_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_encap_nsh *task = (struct task_encap_nsh *)tbase;
	uint8_t out[MAX_PKT_BURST];
	uint16_t j;

	prefetch_first(mbufs, n_pkts);
	for (j = 0; j + PREFETCH_OFFSET < n_pkts; ++j) {
#ifdef PROX_PREFETCH_OFFSET
		PREFETCH0(mbufs[j + PREFETCH_OFFSET]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PREFETCH_OFFSET - 1], void *));
#endif
		out[j] = handle_encap_nsh(task, mbufs[j]);
	}
#ifdef PROX_PREFETCH_OFFSET
	PREFETCH0(rte_pktmbuf_mtod(mbufs[n_pkts - 1], void *));
	for (; j < n_pkts; ++j) {
		out[j] = handle_encap_nsh(task, mbufs[j]);
	}
#endif
	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static struct task_init task_init_decap_nsh = {
	.mode_str = "decapnsh",
	.init = init_task_decap_nsh,
	.handle = handle_decap_nsh_bulk,
	.thread_x = thread_generic,
	.size = sizeof(struct task_decap_nsh)
};

static struct task_init task_init_encap_nsh = {
	.mode_str = "encapnsh",
	.init = init_task_encap_nsh,
	.handle = handle_encap_nsh_bulk,
	.size = sizeof(struct task_encap_nsh)
};

__attribute__((constructor)) static void reg_task_nshtag(void)
{
	reg_task(&task_init_decap_nsh);
	reg_task(&task_init_encap_nsh);
}
