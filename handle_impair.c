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
#include "random.h"

#if RTE_VERSION < RTE_VERSION_NUM(1,8,0,0)
#define RTE_CACHE_LINE_SIZE CACHE_LINE_SIZE
#endif

#define DELAY_ACCURACY	11		// accuracy of 2048 cycles ~= 1 micro-second
#define DELAY_MAX_MASK	0x1FFFFF	// Maximum 2M * 2K cycles ~1 second

struct queue_elem {
	struct rte_mbuf *mbuf;
	uint64_t        tsc;
};

struct queue {
	struct queue_elem *queue_elem;
	unsigned queue_head;
	unsigned queue_tail;
};

struct task_impair {
	struct task_base base;
	struct queue_elem *queue;
	uint64_t delay_time;
	uint64_t delay_time_mask;
	unsigned queue_head;
	unsigned queue_tail;
	unsigned queue_mask;
	int tresh;
	unsigned int seed;
	struct random state;
	uint64_t last_idx;
	struct queue *buffer;
};

static int handle_bulk_random_drop(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_impair *task = (struct task_impair *)tbase;
	uint8_t out[MAX_PKT_BURST];
	for (uint16_t i = 0; i < n_pkts; ++i) {
		out[i] = rand_r(&task->seed) <= task->tresh? 0 : OUT_DISCARD;
	}
	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static int handle_bulk_impair(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_impair *task = (struct task_impair *)tbase;
	uint64_t now = rte_rdtsc();
	uint8_t out[MAX_PKT_BURST] = {0};
	uint16_t enqueue_failed;
	uint16_t i;
	int ret = 0;

	int nb_empty_slots = (task->queue_tail - task->queue_head + task->queue_mask) & task->queue_mask;
	if (likely(nb_empty_slots >= n_pkts)) {
		/* We know n_pkts fits, no need to check for every packet */
		for (i = 0; i < n_pkts; ++i) {
			task->queue[task->queue_head].tsc = now + task->delay_time;
			task->queue[task->queue_head].mbuf = mbufs[i];
			task->queue_head = (task->queue_head + 1) & task->queue_mask;
		}
	} else {
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
				ret+= task->base.tx_pkt(&task->base, mbufs + enqueue_failed,
					  	n_pkts - enqueue_failed, out + enqueue_failed);
				break;
			}
		}
	}

	struct rte_mbuf *new_mbufs[MAX_PKT_BURST];
	uint16_t idx = 0;

	if (task->tresh != RAND_MAX) {
		while (idx < MAX_PKT_BURST && task->queue_tail != task->queue_head) {
			if (task->queue[task->queue_tail].tsc <= now) {
				out[idx] = rand_r(&task->seed) <= task->tresh? 0 : OUT_DISCARD;
				new_mbufs[idx++] = task->queue[task->queue_tail].mbuf;
				task->queue_tail = (task->queue_tail + 1) & task->queue_mask;
			}
			else {
				break;
			}
		}
	} else {
		while (idx < MAX_PKT_BURST && task->queue_tail != task->queue_head) {
			if (task->queue[task->queue_tail].tsc <= now) {
				out[idx] = 0;
				new_mbufs[idx++] = task->queue[task->queue_tail].mbuf;
				task->queue_tail = (task->queue_tail + 1) & task->queue_mask;
			}
			else {
				break;
			}
		}
	}

	ret+= task->base.tx_pkt(&task->base, new_mbufs, idx, out);
	return ret;
}

/*
 * We want to avoid using division and mod for performance reasons.
 * We also want to support up to one second delay, and express it in tsc
 * So the delay in tsc needs up to 32 bits (supposing procesor freq is less than 4GHz).
 * If the max_delay is smaller, we make sure we use less bits.
 * Note that we lose the MSB of the xorshift - 64 bits could hold
 * two or three delays in TSC - but would probably make implementation more complex
 * and not huge gain expected. Maybe room for optimization.
 * Using this implementation, we might have to run random more than once for a delay
 * but in average this should occur less than 50% of the time.
*/

static inline uint64_t random_delay(struct random *state, uint64_t max_delay, uint64_t max_delay_mask)
{
	uint64_t val;
	while(1) {
		val = random_next(state);
		if ((val & max_delay_mask) < max_delay)
			return (val & max_delay_mask);
	}
}

static int handle_bulk_impair_random(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_impair *task = (struct task_impair *)tbase;
	uint64_t now = rte_rdtsc();
	uint8_t out[MAX_PKT_BURST];
	uint16_t enqueue_failed;
	uint16_t i;
	int ret = 0;
	uint64_t packet_time, idx;
	uint64_t now_idx = (now >> DELAY_ACCURACY) & DELAY_MAX_MASK;

	for (i = 0; i < n_pkts; ++i) {
		packet_time = now + random_delay(&task->state, task->delay_time, task->delay_time_mask);
		idx = (packet_time >> DELAY_ACCURACY) & DELAY_MAX_MASK;
		while (idx != ((now_idx - 1) & DELAY_MAX_MASK)) {
			struct queue *queue = &task->buffer[idx];
			if (((queue->queue_head + 1) & task->queue_mask) != queue->queue_tail) {
				queue->queue_elem[queue->queue_head].mbuf = mbufs[i];
				queue->queue_head = (queue->queue_head + 1) & task->queue_mask;
				break;
			} else {
				idx = (idx + 1) & DELAY_MAX_MASK;
			}
		}
		if (idx == ((now_idx - 1) & DELAY_MAX_MASK)) {
			/* Rest does not fit, need to drop packet. Note that further packets might fit as might want to be sent earlier */
			out[0] = OUT_DISCARD;
			ret+= task->base.tx_pkt(&task->base, mbufs + i, 1, out);
			plog_warn("Unexpectdly dropping packets\n");
		}
	}

	struct rte_mbuf *new_mbufs[MAX_PKT_BURST];
	uint16_t pkt_idx = 0;

	while ((pkt_idx < MAX_PKT_BURST) && (task->last_idx != ((now_idx - 1) & DELAY_MAX_MASK))) {
		struct queue *queue = &task->buffer[task->last_idx];
		while ((pkt_idx < MAX_PKT_BURST) && (queue->queue_tail != queue->queue_head)) {
			out[pkt_idx] = rand_r(&task->seed) <= task->tresh? 0 : OUT_DISCARD;
			new_mbufs[pkt_idx++] = queue->queue_elem[queue->queue_tail].mbuf;
			queue->queue_tail = (queue->queue_tail + 1) & task->queue_mask;
		}
		task->last_idx = (task->last_idx + 1) & DELAY_MAX_MASK;
	}

	ret+= task->base.tx_pkt(&task->base, new_mbufs, pkt_idx, out);
	return ret;
}

static void init_task(struct task_base *tbase, struct task_args *targ)
{
	struct task_impair *task = (struct task_impair *)tbase;
	uint32_t queue_len = 0;
	size_t mem_size;
	unsigned socket_id;
	uint64_t delay_us = 0;

	task->seed = rte_rdtsc();
	if (targ->probability == 0)
		targ->probability = 1000000;

	task->tresh = ((uint64_t) RAND_MAX) * targ->probability / 1000000;

	if ((targ->delay_us == 0) && (targ->random_delay_us == 0)) {
		tbase->handle_bulk = handle_bulk_random_drop;
		task->delay_time = 0;
	} else if (targ->random_delay_us) {
		tbase->handle_bulk = handle_bulk_impair_random;
		task->delay_time = usec_to_tsc(targ->random_delay_us);
		task->delay_time_mask = rte_align32pow2(task->delay_time) - 1;
		delay_us = targ->random_delay_us;
		queue_len = rte_align32pow2((1250L * delay_us) / 84 / (DELAY_MAX_MASK + 1));
	} else {
		task->delay_time = usec_to_tsc(targ->delay_us);
		delay_us = targ->delay_us;
		queue_len = rte_align32pow2(1250 * delay_us / 84);
	}
	/* Assume Line-rate is maximum transmit speed.
   	   TODO: take link speed if tx is port.
	*/
	if (queue_len < MAX_PKT_BURST)
		queue_len= MAX_PKT_BURST;
	task->queue_mask = queue_len - 1;
	if (task->queue_mask < MAX_PKT_BURST - 1)
		task->queue_mask = MAX_PKT_BURST - 1;

	mem_size = (task->queue_mask + 1) * sizeof(task->queue[0]);
	socket_id = rte_lcore_to_socket_id(targ->lconf->id);

	if (targ->delay_us) {
		task->queue = prox_zmalloc(mem_size, socket_id);
		task->queue_head = 0;
		task->queue_tail = 0;
	} else if (targ->random_delay_us) {
		size_t size = (DELAY_MAX_MASK + 1) * sizeof(struct queue);
		plog_info("Allocating %zd bytes\n", size);
		task->buffer = prox_zmalloc(size, socket_id);
		PROX_PANIC(task->buffer == NULL, "Not enough memory to allocate buffer\n");
		plog_info("Allocating %d x %zd bytes\n", DELAY_MAX_MASK + 1, mem_size);

		for (int i = 0; i < DELAY_MAX_MASK + 1; i++) {
			task->buffer[i].queue_elem = prox_zmalloc(mem_size, socket_id);
			PROX_PANIC(task->buffer[i].queue_elem == NULL, "Not enough memory to allocate buffer elems\n");
		}
	}
	random_init_seed(&task->state);
}

static struct task_init tinit = {
	.mode_str = "impair",
	.init = init_task,
	.handle = handle_bulk_impair,
	.flag_features = TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS | TASK_FEATURE_ZERO_RX,
	.size = sizeof(struct task_impair)
};

__attribute__((constructor)) static void ctor(void)
{
	reg_task(&tinit);
}
