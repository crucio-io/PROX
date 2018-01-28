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

#include <rte_mbuf.h>
#include <rte_cycles.h>

#include "task_base.h"
#include "task_init.h"
#include "thread_generic.h"

struct task_tsc {
	struct task_base base;
};

static int handle_bulk_tsc(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_tsc *task = (struct task_tsc *)tbase;
	const uint64_t rx_tsc = rte_rdtsc();

	for (uint16_t j = 0; j < n_pkts; ++j)
		mbufs[j]->udata64 = rx_tsc;

	return task->base.tx_pkt(&task->base, mbufs, n_pkts, NULL);
}

static struct task_init task_init = {
	.mode_str = "tsc",
	.init = NULL,
	.handle = handle_bulk_tsc,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS|TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS|TASK_FEATURE_TXQ_FLAGS_NOMULTSEGS|TASK_FEATURE_THROUGHPUT_OPT,
	.size = sizeof(struct task_tsc),
	.mbuf_size = 2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM,
};

__attribute__((constructor)) static void reg_task_nop(void)
{
	reg_task(&task_init);
}
