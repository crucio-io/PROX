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

#include <rte_ip.h>

#include "log.h"
#include "tx_pkt.h"
#include "task_base.h"
#include "task_init.h"
#include "mpls.h"
#include "defines.h"
#include "prefetch.h"
#include "qinq.h"
#include "prox_assert.h"
#include "etypes.h"

struct task_untag {
	struct task_base base;
	uint16_t         etype;
};

static void init_task_untag(struct task_base *tbase, __attribute__((unused)) struct task_args *targ)
{
	struct task_untag *task = (struct task_untag *)tbase;
	task->etype = targ->etype;
}

static inline uint8_t handle_untag(struct task_untag *task, struct rte_mbuf *mbuf);

static int handle_untag_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_untag *task = (struct task_untag *)tbase;
	uint8_t out[MAX_PKT_BURST];
	uint16_t j;

	prefetch_first(mbufs, n_pkts);

	for (j = 0; j + PREFETCH_OFFSET < n_pkts; ++j) {
#ifdef PROX_PREFETCH_OFFSET
		PREFETCH0(mbufs[j + PREFETCH_OFFSET]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PREFETCH_OFFSET - 1], void *));
#endif
		out[j] = handle_untag(task, mbufs[j]);
	}
#ifdef PROX_PREFETCH_OFFSET
	PREFETCH0(rte_pktmbuf_mtod(mbufs[n_pkts - 1], void *));
	for (; j < n_pkts; ++j) {
		out[j] = handle_untag(task, mbufs[j]);
	}
#endif

	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static inline uint8_t untag_mpls(struct rte_mbuf *mbuf, struct ether_hdr *peth)
{
	struct ether_hdr *pneweth = (struct ether_hdr *)rte_pktmbuf_adj(mbuf, 4);
	const struct mpls_hdr *mpls = (const struct mpls_hdr *)(peth + 1);
	const struct ipv4_hdr *pip = (const struct ipv4_hdr *)(mpls + 1);
	PROX_ASSERT(pneweth);

	if (mpls->bos == 0) {
		// Double MPLS tag
		pneweth = (struct ether_hdr *)rte_pktmbuf_adj(mbuf, 4);
		PROX_ASSERT(pneweth);
	}

	if ((pip->version_ihl >> 4) == 4) {
		pneweth->ether_type = ETYPE_IPv4;
		return 0;
	}
	else if ((pip->version_ihl >> 4) == 6) {
		pneweth->ether_type = ETYPE_IPv6;
		return 0;
	}

	plog_warn("Failed Decoding MPLS Packet - neither IPv4 neither IPv6: version %u\n", pip->version_ihl);
	return OUT_DISCARD;
}

static uint8_t untag_qinq(struct rte_mbuf *mbuf, struct qinq_hdr *qinq)
{
	if ((qinq->cvlan.eth_proto != ETYPE_VLAN)) {
		plog_warn("Unexpected proto in QinQ = %#04x\n", qinq->cvlan.eth_proto);
		return OUT_DISCARD;
	}

	rte_pktmbuf_adj(mbuf, sizeof(struct qinq_hdr) - sizeof(struct ether_hdr));
	return 0;
}

static inline uint8_t handle_untag(struct task_untag *task, struct rte_mbuf *mbuf)
{
	struct ether_hdr *peth = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
	const uint16_t etype = peth->ether_type;

	if (etype != task->etype) {
		plog_warn("Failed Removing %04x tag: ether_type = %#06x\n", task->etype, peth->ether_type);
		return OUT_DISCARD;
	}

	switch (etype) {
	case ETYPE_MPLSU:
		/* MPLS Decapsulation */
		return untag_mpls(mbuf, peth);
	case ETYPE_LLDP:
		return OUT_DISCARD;
	case ETYPE_IPv6:
		return 0;
	case ETYPE_IPv4:
		return 0;
	case ETYPE_8021ad:
	case ETYPE_VLAN:
		return untag_qinq(mbuf, (struct qinq_hdr *)peth);
	default:
		plog_warn("Failed untagging header: ether_type = %#06x is not supported\n", peth->ether_type);
		return OUT_DISCARD;
	}
}

static struct task_init task_init_untag = {
	.mode_str = "untag",
	.init = init_task_untag,
	.handle = handle_untag_bulk,
	.size = sizeof(struct task_untag)
};

__attribute__((constructor)) static void reg_task_untag(void)
{
	reg_task(&task_init_untag);
}
