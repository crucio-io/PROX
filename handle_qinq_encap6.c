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

#include <rte_table_hash.h>

#include "handle_qinq_encap6.h"
#include "handle_qinq_encap4.h"
#include "task_base.h"
#include "qinq.h"
#include "defines.h"
#include "tx_pkt.h"
#include "hash_entry_types.h"
#include "prefetch.h"
#include "log.h"
#include "lconf.h"
#include "mpls.h"
#include "hash_utils.h"
#include "quit.h"

struct task_qinq_encap6 {
	struct task_base                    base;
	uint16_t                            qinq_tag;
	uint8_t				    tx_portid;
	uint8_t                             runtime_flags;
	struct rte_table_hash               *cpe_table;
};

static void init_task_qinq_encap6(struct task_base *tbase, struct task_args *targ)
{
	struct task_qinq_encap6 *task = (struct task_qinq_encap6 *)tbase;

	task->qinq_tag = targ->qinq_tag;
	task->cpe_table = targ->cpe_table;
	task->runtime_flags = targ->runtime_flags;
}

/* Encapsulate IPv6 packet in QinQ where the QinQ is derived from the IPv6 address */
static inline uint8_t handle_qinq_encap6(struct rte_mbuf *mbuf, struct task_qinq_encap6 *task)
{
	struct qinq_hdr *pqinq = (struct qinq_hdr *)rte_pktmbuf_prepend(mbuf, 2 * sizeof(struct vlan_hdr));

	PROX_ASSERT(pqinq);
	struct ipv6_hdr *pip6 = (struct ipv6_hdr *)(pqinq + 1);

	if (pip6->hop_limits) {
		pip6->hop_limits--;
	}
	else {
		plog_info("TTL = 0 => Dropping\n");
		return OUT_DISCARD;
	}

	// TODO: optimize to use bulk as intended with the rte_table_library
	uint64_t pkts_mask = RTE_LEN2MASK(1, uint64_t);
	uint64_t lookup_hit_mask;
	struct cpe_data* entries[64]; // TODO: use bulk size
	rte_table_hash_ext_dosig_ops.f_lookup(task->cpe_table, &mbuf, pkts_mask, &lookup_hit_mask, (void**)entries);

	if (lookup_hit_mask == 0x1) {
		/* will also overwrite part of the destination addr */
		(*(uint64_t *)pqinq) = entries[0]->mac_port_8bytes;
		pqinq->svlan.eth_proto = task->qinq_tag;
		pqinq->cvlan.eth_proto = ETYPE_VLAN;
		pqinq->svlan.vlan_tci = entries[0]->qinq_svlan;
		pqinq->cvlan.vlan_tci = entries[0]->qinq_cvlan;
		pqinq->ether_type = ETYPE_IPv6;

		/* classification can only be done from this point */
		if (task->runtime_flags & TASK_CLASSIFY) {
			rte_sched_port_pkt_write(mbuf, 0, entries[0]->user, 0, 0, 0);
		}
		return 0;
	}
	else {
		plogx_err("Unknown IP " IPv6_BYTES_FMT "\n", IPv6_BYTES(pip6->dst_addr));
		return OUT_DISCARD;
	}
}

void init_cpe6_table(struct task_args *targ)
{
	char name[64];
	sprintf(name, "core_%u_CPEv6Table", targ->lconf->id);

	uint8_t table_part = targ->nb_slave_threads;
	if (!rte_is_power_of_2(table_part)) {
		table_part = rte_align32pow2(table_part) >> 1;
	}

	uint32_t n_entries = MAX_GRE / table_part;
	struct rte_table_hash_ext_params table_hash_params = {
		.key_size = sizeof(struct ipv6_addr),
		.n_keys = n_entries,
		.n_buckets = n_entries >> 2,
		.n_buckets_ext = n_entries >> 3,
		.f_hash = hash_crc32,
		.seed = 0,
		.signature_offset = HASH_METADATA_OFFSET(0),
		.key_offset = HASH_METADATA_OFFSET(0),
	};

	size_t entry_size = sizeof(struct cpe_data);
	if (!rte_is_power_of_2(entry_size)) {
		entry_size = rte_align32pow2(entry_size);
	}

	struct rte_table_hash* phash = rte_table_hash_ext_dosig_ops.
		f_create(&table_hash_params, rte_lcore_to_socket_id(targ->lconf->id), entry_size);
	PROX_PANIC(phash == NULL, "Unable to allocate memory for IPv6 hash table on core %u\n", targ->lconf->id);

	for (uint8_t task_id = 0; task_id < targ->lconf->n_tasks_all; ++task_id) {
		enum task_mode smode = targ->lconf->targs[task_id].mode;
		if (smode == QINQ_DECAP6 || smode == QINQ_ENCAP6) {
			targ->lconf->targs[task_id].cpe_table = phash;
		}
	}
}

static void early_init(struct task_args *targ)
{
	if (!targ->cpe_table) {
		init_cpe6_table(targ);
	}
}

static int handle_qinq_encap6_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_qinq_encap6 *task = (struct task_qinq_encap6 *)tbase;
	uint8_t out[MAX_PKT_BURST];
	uint16_t j;

        prefetch_first(mbufs, n_pkts);

	for (j = 0; j + PREFETCH_OFFSET < n_pkts; ++j) {
#ifdef PROX_PREFETCH_OFFSET
		PREFETCH0(mbufs[j + PREFETCH_OFFSET]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PREFETCH_OFFSET - 1], void *));
#endif
		out[j] = handle_qinq_encap6(mbufs[j], task);
	}
#ifdef PROX_PREFETCH_OFFSET
	PREFETCH0(rte_pktmbuf_mtod(mbufs[n_pkts - 1], void *));
	for (; j < n_pkts; ++j) {
		out[j] = handle_qinq_encap6(mbufs[j], task);
	}
#endif

	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static int handle_qinq_encap6_untag_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_qinq_encap6 *task = (struct task_qinq_encap6 *)tbase;
	uint8_t out[MAX_PKT_BURST];
	uint16_t j;

	prefetch_first(mbufs, n_pkts);

	for (j = 0; j + PREFETCH_OFFSET < n_pkts; ++j) {
#ifdef PROX_PREFETCH_OFFSET
		PREFETCH0(mbufs[j + PREFETCH_OFFSET]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PREFETCH_OFFSET - 1], void *));
#endif
		if (likely(mpls_untag(mbufs[j]))) {
			out[j] = handle_qinq_encap6(mbufs[j], task);
		}
		else {
			out[j] = OUT_DISCARD;
		}
	}
#ifdef PROX_PREFETCH_OFFSET
	PREFETCH0(rte_pktmbuf_mtod(mbufs[n_pkts - 1], void *));
	for (; j < n_pkts; ++j) {
		if (likely(mpls_untag(mbufs[j]))) {
			out[j] = handle_qinq_encap6(mbufs[j], task);
		}
		else {
			out[j] = OUT_DISCARD;
		}
	}
#endif

	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static struct task_init task_init_qinq_encap6 = {
	.mode = QINQ_ENCAP6,
	.mode_str = "qinqencapv6",
	.init = init_task_qinq_encap6,
	.early_init = early_init,
	.handle = handle_qinq_encap6_bulk,
	.flag_features = TASK_FEATURE_CLASSIFY,
	.size = sizeof(struct task_qinq_encap6)
};

static struct task_init task_init_qinq_encap6_untag = {
	.mode = QINQ_ENCAP6,
	.mode_str = "qinqencapv6",
	.sub_mode_str = "unmpls",
	.early_init = early_init,
	.init = init_task_qinq_encap6,
	.handle = handle_qinq_encap6_untag_bulk,
	.flag_features = TASK_FEATURE_CLASSIFY,
	.size = sizeof(struct task_qinq_encap6)
};

__attribute__((constructor)) static void reg_task_qinq_encap6(void)
{
	reg_task(&task_init_qinq_encap6);
	reg_task(&task_init_qinq_encap6_untag);
}
