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

#include <string.h>
#include <stdio.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_version.h>

#include "prox_lua.h"
#include "prox_lua_types.h"
#include "prox_malloc.h"
#include "task_base.h"
#include "task_init.h"
#include "lconf.h"
#include "prefetch.h"
#include "quit.h"
#include "log.h"
#include "defines.h"
#include "qinq.h"
#include "prox_cfg.h"
#include "prox_shared.h"

#if RTE_VERSION < RTE_VERSION_NUM(1,8,0,0)
#define RTE_CACHE_LINE_SIZE CACHE_LINE_SIZE
#endif

struct task_police {
	struct task_base base;
	union {
		struct rte_meter_srtcm *sr_flows;
		struct rte_meter_trtcm *tr_flows;
	};

	uint16_t           *user_table;
	enum police_action police_act[3][3];
	uint16_t overhead;
	uint8_t runtime_flags;
};

typedef uint8_t (*hp) (struct task_police *task, struct rte_mbuf *mbuf, uint64_t tsc, uint32_t user);

static uint8_t handle_police(struct task_police *task, struct rte_mbuf *mbuf, uint64_t tsc, uint32_t user)
{
	enum rte_meter_color in_color = e_RTE_METER_GREEN;
	enum rte_meter_color out_color;
	uint32_t pkt_len = rte_pktmbuf_pkt_len(mbuf) + task->overhead;
	out_color = rte_meter_srtcm_color_aware_check(&task->sr_flows[user], tsc, pkt_len, in_color);

	return task->police_act[in_color][out_color] == ACT_DROP? OUT_DISCARD : 0;
}

static uint8_t handle_police_tr(struct task_police *task, struct rte_mbuf *mbuf, uint64_t tsc, uint32_t user)
{
	enum rte_meter_color in_color = e_RTE_METER_GREEN;
	enum rte_meter_color out_color;
	uint32_t pkt_len = rte_pktmbuf_pkt_len(mbuf) + task->overhead;
	out_color = rte_meter_trtcm_color_aware_check(&task->tr_flows[user], tsc, pkt_len, in_color);

	if (task->runtime_flags  & TASK_MARK) {
#if RTE_VERSION >= RTE_VERSION_NUM(1,8,0,0)
		uint32_t subport, pipe, traffic_class, queue;
		enum rte_meter_color color;

		rte_sched_port_pkt_read_tree_path(mbuf, &subport, &pipe, &traffic_class, &queue);
		color = task->police_act[in_color][out_color];

		rte_sched_port_pkt_write(mbuf, subport, pipe, traffic_class, queue, color);
#else
		struct rte_sched_port_hierarchy *sched =
			(struct rte_sched_port_hierarchy *) &mbuf->pkt.hash.sched;
		sched->color = task->police_act[in_color][out_color];
#endif
	}

	return task->police_act[in_color][out_color] == ACT_DROP? OUT_DISCARD : 0;
}

static inline int get_user(struct task_police *task, struct rte_mbuf *mbuf)
{
	if (task->runtime_flags & TASK_CLASSIFY) {
		struct qinq_hdr *pqinq = rte_pktmbuf_mtod(mbuf, struct qinq_hdr *);
		return PKT_TO_LUTQINQ(pqinq->svlan.vlan_tci, pqinq->cvlan.vlan_tci);
	}

#if RTE_VERSION >= RTE_VERSION_NUM(1,8,0,0)
	uint32_t dummy;
	uint32_t pipe;

	rte_sched_port_pkt_read_tree_path(mbuf, &dummy, &pipe, &dummy, &dummy);
	return pipe;
#else
	struct rte_sched_port_hierarchy *sched =
		(struct rte_sched_port_hierarchy *) &mbuf->pkt.hash.sched;
	return sched->pipe;
#endif
}

#define PHASE1_DELAY PREFETCH_OFFSET
#define PHASE2_DELAY PREFETCH_OFFSET
#define PHASE3_DELAY PREFETCH_OFFSET
#define PHASE4_DELAY PREFETCH_OFFSET

static inline int handle_pb(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts, hp handle_police_func)
{
	struct task_police *task = (struct task_police *)tbase;
	uint16_t j;
	uint64_t cur_tsc = rte_rdtsc();
	uint32_t user[64];
	uint8_t  out[MAX_PKT_BURST];
	uint32_t cur_user;
	for (j = 0; j < PHASE1_DELAY && j < n_pkts; ++j) {
		PREFETCH0(mbufs[j]);
	}

	for (j = 0; j < PHASE2_DELAY && j + PHASE1_DELAY < n_pkts; ++j) {
		PREFETCH0(mbufs[j + PHASE1_DELAY]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j], void*));
	}

	for (j = 0; j < PHASE3_DELAY && j + PHASE2_DELAY + PHASE1_DELAY < n_pkts; ++j) {
		PREFETCH0(mbufs[j + PHASE2_DELAY + PHASE1_DELAY]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PHASE2_DELAY], void*));
                cur_user = get_user(task, mbufs[j]);
		user[j] = cur_user;
		PREFETCH0(&task->user_table[cur_user]);
	}

	/* At this point, the whole pipeline is running */
	for (j = 0; j + PHASE3_DELAY + PHASE2_DELAY + PHASE1_DELAY < n_pkts; ++j) {
		PREFETCH0(mbufs[j + PHASE3_DELAY + PHASE2_DELAY + PHASE1_DELAY]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PHASE3_DELAY + PHASE2_DELAY], void*));
		cur_user = get_user(task, mbufs[j + PHASE3_DELAY]);
		user[j + PHASE3_DELAY] = cur_user;
		PREFETCH0(&task->user_table[cur_user]);

		out[j] = handle_police_func(task, mbufs[j], cur_tsc, task->user_table[user[j]]);
	}

	/* Last part of pipeline */
	for (; j + PHASE3_DELAY + PHASE2_DELAY < n_pkts; ++j) {
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j + PHASE3_DELAY + PHASE2_DELAY], void*));
		PREFETCH0(&task->user_table[j + PHASE3_DELAY]);
		cur_user = get_user(task, mbufs[j + PHASE3_DELAY]);
		user[j + PHASE3_DELAY] = cur_user;
		PREFETCH0(&task->user_table[cur_user]);

		out[j] = handle_police_func(task, mbufs[j], cur_tsc, task->user_table[user[j]]);
	}

	for (; j + PHASE3_DELAY < n_pkts; ++j) {
		cur_user = get_user(task, mbufs[j + PHASE3_DELAY]);
		user[j + PHASE3_DELAY] = cur_user;
		PREFETCH0(&task->user_table[cur_user]);

		out[j] = handle_police_func(task, mbufs[j], cur_tsc, task->user_table[user[j]]);
	}

	for (; j < n_pkts; ++j) {
		out[j] = handle_police_func(task, mbufs[j], cur_tsc, task->user_table[user[j]]);
	}

	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static int handle_police_bulk(struct task_base *tbase, struct rte_mbuf **mbuf, uint16_t n_pkts)
{
        return handle_pb(tbase, mbuf, n_pkts, handle_police);
}

static int handle_police_tr_bulk(struct task_base *tbase, struct rte_mbuf **mbuf, uint16_t n_pkts)
{
        return handle_pb(tbase, mbuf, n_pkts, handle_police_tr);
}

static void init_task_police(struct task_base *tbase, struct task_args *targ)
{
	struct task_police *task = (struct task_police *)tbase;
	const int socket_id = rte_lcore_to_socket_id(targ->lconf->id);

	task->overhead = targ->overhead;
	task->runtime_flags = targ->runtime_flags;

	task->user_table = prox_sh_find_socket(socket_id, "user_table");
	if (!task->user_table) {
		PROX_PANIC(!strcmp(targ->user_table, ""), "No user table defined\n");
		int ret = lua_to_user_table(prox_lua(), GLOBAL, targ->user_table, socket_id, &task->user_table);
		PROX_PANIC(ret, "Failed to create user table from config:\n%s\n", get_lua_to_errors());
		prox_sh_add_socket(socket_id, "user_table", task->user_table);
	}

	if (strcmp(targ->task_init->sub_mode_str, "trtcm")) {
		task->sr_flows = prox_zmalloc(targ->n_flows * sizeof(*task->sr_flows), socket_id);
		PROX_PANIC(task->sr_flows == NULL, "Failed to allocate flow contexts\n");
		PROX_PANIC(!targ->cir, "Commited information rate is set to 0\n");
		PROX_PANIC(!targ->cbs, "Commited information bucket size is set to 0\n");
		PROX_PANIC(!targ->ebs, "Execess information bucket size is set to 0\n");

		struct rte_meter_srtcm_params params = {
			.cir = targ->cir,
			.cbs = targ->cbs,
			.ebs = targ->ebs,
		};

		for (uint32_t i = 0; i < targ->n_flows; ++i) {
			rte_meter_srtcm_config(&task->sr_flows[i], &params);
		}
	}
	else {
		task->tr_flows = prox_zmalloc(targ->n_flows * sizeof(*task->tr_flows), socket_id);
		PROX_PANIC(task->tr_flows == NULL, "Failed to allocate flow contexts\n");
		PROX_PANIC(!targ->pir, "Peak information rate is set to 0\n");
		PROX_PANIC(!targ->cir, "Commited information rate is set to 0\n");
		PROX_PANIC(!targ->pbs, "Peak information bucket size is set to 0\n");
		PROX_PANIC(!targ->cbs, "Commited information bucket size is set to 0\n");

		struct rte_meter_trtcm_params params = {
			.pir = targ->pir,
			.pbs = targ->pbs,
			.cir = targ->cir,
			.cbs = targ->cbs,
		};

		for (uint32_t i = 0; i < targ->n_flows; ++i) {
			rte_meter_trtcm_config(&task->tr_flows[i], &params);
		}
	}

	for (uint32_t i = 0; i < 3; ++i) {
		for (uint32_t j = 0; j < 3; ++j) {
			task->police_act[i][j] = targ->police_act[i][j];
		}
	}
}

static struct task_init task_init_police = {
	.mode_str = "police",
	.init = init_task_police,
	.handle = handle_police_bulk,
	.flag_features = TASK_FEATURE_CLASSIFY,
	.size = sizeof(struct task_police)
};

static struct task_init task_init_police2 = {
	.mode_str = "police",
	.sub_mode_str = "trtcm",
	.init = init_task_police,
	.handle = handle_police_tr_bulk,
	.flag_features = TASK_FEATURE_CLASSIFY,
	.size = sizeof(struct task_police)
};

__attribute__((constructor)) static void reg_task_police(void)
{
	reg_task(&task_init_police);
	reg_task(&task_init_police2);
}
