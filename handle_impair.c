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
#include <rte_cycles.h>
#include <rte_version.h>

#include "prox_malloc.h"
#include "lconf.h"
#include "log.h"

#if RTE_VERSION < RTE_VERSION_NUM(1,8,0,0)
#define RTE_CACHE_LINE_SIZE CACHE_LINE_SIZE
#endif

struct task_impair {
	struct task_base base;
	int tresh;
	unsigned int seed;
};

struct queue_elem {
	struct rte_mbuf *mbuf;
	uint64_t        tsc;
};

struct task_impair2 {
	struct task_base base;
	struct queue_elem *queue;
	uint64_t delay_time;
	unsigned queue_head;
	unsigned queue_tail;
	unsigned queue_mask;
};

static void init_task(struct task_base *tbase, struct task_args *targ)
{
	struct task_impair *task = (struct task_impair *)tbase;

	task->seed = rte_rdtsc();
	task->tresh = ((uint64_t) RAND_MAX) * targ->probability / 100;
}

static void handle_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_impair *task = (struct task_impair *)tbase;
	uint8_t out[MAX_PKT_BURST];

	for (uint16_t i = 0; i < n_pkts; ++i) {
		out[i] = rand_r(&task->seed) <= task->tresh? 0 : OUT_DISCARD;
	}

	task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static void init_task2(struct task_base *tbase, struct task_args *targ)
{
	struct task_impair2 *task = (struct task_impair2 *)tbase;
	uint32_t queue_len = 0;
	size_t mem_size;
	unsigned socket_id;

	task->delay_time = msec_to_tsc(targ->delay_ms);

	/* Assume Line-rate is maximum transmit speed.
	   TODO: take link speed if tx is port. */
	queue_len = rte_align32pow2(1250000 * targ->delay_ms / 84);

	if (queue_len < MAX_PKT_BURST)
		queue_len= MAX_PKT_BURST;
	task->queue_mask = queue_len - 1;

	if (task->queue_mask < MAX_PKT_BURST - 1)
		task->queue_mask = MAX_PKT_BURST - 1;

	mem_size = (task->queue_mask + 1) * sizeof(task->queue[0]);
	socket_id = rte_lcore_to_socket_id(targ->lconf->id);

	task->queue = prox_zmalloc(mem_size, socket_id);
	task->queue_head = 0;
	task->queue_tail = 0;
}

static void handle_bulk_delay(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_impair2 *task = (struct task_impair2 *)tbase;
	uint64_t now = rte_rdtsc();
	uint8_t out[MAX_PKT_BURST];
	uint16_t enqueue_failed;
	uint16_t i;

	for (i = 0; i < n_pkts; ++i) {
		if (((task->queue_head + 1) & task->queue_mask) != task->queue_tail) {
			task->queue[task->queue_head].tsc = now + task->delay_time;
			task->queue[task->queue_head].mbuf = mbufs[i];
			task->queue_head = (task->queue_head + 1) & task->queue_mask;
		}
		else {
			/* Rest does not fit, need to drop those packets. */
			enqueue_failed = i;
			for (;i < n_pkts; ++i) {
				out[i] = OUT_DISCARD;
			}
			task->base.tx_pkt(&task->base, mbufs + enqueue_failed,
					  n_pkts - enqueue_failed, out + enqueue_failed);
			break;
		}
	}

	struct rte_mbuf *new_mbufs[MAX_PKT_BURST];
	uint16_t idx = 0;

	while (idx < MAX_PKT_BURST && task->queue_tail != task->queue_head) {
		if (task->queue[task->queue_tail].tsc <= now) {
			out[idx] = 0;
			new_mbufs[idx++] = task->queue[task->queue_tail].mbuf;

			task->queue_tail = (task->queue_tail + 1) & task->queue_mask;
		}
		else {
			break;
		}
	};

	task->base.tx_pkt(&task->base, new_mbufs, idx, out);
}

static struct task_init tinit = {
	.mode_str = "impair",
	.init = init_task,
	.handle = handle_bulk,
	.flag_features = TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS,
	.size = sizeof(struct task_impair)
};

static struct task_init tinit2 = {
	.mode_str = "impair",
	.sub_mode_str = "delay",
	.init = init_task2,
	.handle = handle_bulk_delay,
	.flag_features = TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS | TASK_FEATURE_ZERO_RX,
	.size = sizeof(struct task_impair2)
};

__attribute__((constructor)) static void ctor(void)
{
	reg_task(&tinit);
	reg_task(&tinit2);
}
