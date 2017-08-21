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

#include "lconf.h"
#include "task_base.h"
#include "task_init.h"
#include "handle_irq.h"
#include "log.h"
#include "unistd.h"
#include "input.h"

#define MAX_INDEX	65535 * 16

struct irq_info {
	uint64_t tsc;
	uint64_t lat;
};

struct irq_bucket {
	uint64_t index;
	struct irq_info info[MAX_INDEX];
};

struct task_irq {
	struct task_base base;
	uint64_t start_tsc;
	uint64_t tsc;
	uint64_t max_irq;
	uint8_t  lcore_id;
	volatile uint16_t stats_use_lt; /* which lt to use, */
	volatile uint16_t task_use_lt; /* 0 or 1 depending on which of the 2 result records are used */
	struct irq_bucket buffer[2];
};

#define MAX_INTERRUPT_LENGTH	500000	/* Maximum length of an interrupt is (1 / MAX_INTERRUPT_LENGTH) seconds */

/*
 *	This module is not handling any packets.
 *	It loops on rdtsc() and checks whether it has been interrupted
 *		 for more than (1 / MAX_INTERRUPT_LENGTH) sec.
 *	This is a debugging only task, useful to check if the system h
 *		as been properly configured.
*/

void task_irq_show_stats(struct task_irq *task_irq, struct input *input)
{
	struct irq_bucket *bucket = &task_irq->buffer[!task_irq->task_use_lt];
	if (input->reply) {
		char buf[8192] = {0};
		if (bucket->index == 0) {
			sprintf(buf, "\n");
			input->reply(input, buf, strlen(buf));
			buf[0] = 0;
		}
		for (uint64_t i = 0; i < bucket->index; i++) {
			sprintf(buf + strlen(buf), "%d; %"PRIu64"""; %ld; %ld; %ld; %ld ;",
				task_irq->lcore_id,
				i,
				bucket->info[i].lat,
				bucket->info[i].lat * 1000000 / rte_get_tsc_hz(),
				bucket->info[i].tsc - task_irq->start_tsc,
				(bucket->info[i].tsc - task_irq->start_tsc) * 1000 / rte_get_tsc_hz());
			sprintf(buf+strlen(buf), "\n");
			input->reply(input, buf, strlen(buf));
			buf[0] = 0;
		}
	} else {
		for (uint64_t i = 0; i < bucket->index; i++)
			if (bucket->info[i].lat)
				plog_info("[%d]; Interrupt %"PRIu64": %ld cycles (%ld micro-sec) at %ld cycles (%ld msec)\n",
					  task_irq->lcore_id,
					  i,
					  bucket->info[i].lat,
					  bucket->info[i].lat * 1000000 / rte_get_tsc_hz(),
					  bucket->info[i].tsc - task_irq->start_tsc,
					  (bucket->info[i].tsc - task_irq->start_tsc) * 1000 / rte_get_tsc_hz());
	}
	task_irq->stats_use_lt = !task_irq->task_use_lt;
	bucket->index = 0;
}

static void irq_stop(struct task_base *tbase)
{
	struct task_irq *task = (struct task_irq *)tbase;
	uint32_t i;
	uint32_t lcore_id = rte_lcore_id();
	int bucket_id;

	plog_info("Stopping core %u\n", lcore_id);
	plog_info("Core ID; Interrupt (nanosec); Time (msec)\n");
	for (int j = 0; j < 2; j++) {
		// Start dumping the oldest bucket first
		if (task->buffer[0].info[0].tsc < task->buffer[1].info[0].tsc)
			bucket_id = j;
		else
			bucket_id = !j;
		struct irq_bucket *bucket = &task->buffer[bucket_id];
		for (i=0; i< bucket->index;i++) {
			if (bucket->info[i].lat != 0) {
				plog_info("%d; %ld; %ld\n",
					  lcore_id,
					  bucket->info[i].lat * 1000000000 / rte_get_tsc_hz(),
					  (bucket->info[i].tsc - task->start_tsc) * 1000 / rte_get_tsc_hz());
			}
		}
	}
	plog_info("Core %u stopped\n", lcore_id);
}

static inline int handle_irq_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_irq *task = (struct task_irq *)tbase;
	uint64_t tsc1;
	uint64_t index;

	if (task->stats_use_lt != task->task_use_lt)
		task->task_use_lt = task->stats_use_lt;
	struct irq_bucket *bucket = &task->buffer[task->task_use_lt];

	tsc1 = rte_rdtsc();
	if ((task->tsc != 0) && ((tsc1 - task->tsc) > task->max_irq) && (bucket->index < MAX_INDEX)) {
		bucket->info[bucket->index].tsc = tsc1;
		bucket->info[bucket->index++].lat = tsc1 - task->tsc;
	}
	task->tsc = tsc1;
	return 0;
}

static void init_task_irq(struct task_base *tbase,
			  __attribute__((unused)) struct task_args *targ)
{
	struct task_irq *task = (struct task_irq *)tbase;
	// max_irq expressed in cycles
	task->max_irq = rte_get_tsc_hz() / MAX_INTERRUPT_LENGTH;
	task->start_tsc = rte_rdtsc();
	task->lcore_id = targ->lconf->id;
	plog_info("\tusing irq mode with max irq set to %ld cycles\n", task->max_irq);
}

static struct task_init task_init_irq = {
	.mode_str = "irq",
	.init = init_task_irq,
	.handle = handle_irq_bulk,
	.stop = irq_stop,
	.flag_features = TASK_FEATURE_NO_RX,
	.size = sizeof(struct task_irq)
};

static struct task_init task_init_none;

__attribute__((constructor)) static void reg_task_irq(void)
{
	reg_task(&task_init_irq);
}
