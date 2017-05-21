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

#include "handle_nop.h"
#include "thread_nop.h"

static struct task_init task_init_nop_thrpt_opt = {
	.mode_str = "nop",
	.init = NULL,
	.handle = handle_nop_bulk,
	.thread_x = thread_nop,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS|TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS|TASK_FEATURE_TXQ_FLAGS_NOMULTSEGS|TASK_FEATURE_THROUGHPUT_OPT|TASK_FEATURE_MULTI_RX,
	.size = sizeof(struct task_nop),
	.mbuf_size = 2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM,
};

static struct task_init task_init_nop_lat_opt = {
	.mode_str = "nop",
	.sub_mode_str = "latency optimized",
	.init = NULL,
	.handle = handle_nop_bulk,
	.thread_x = thread_nop,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS|TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS|TASK_FEATURE_TXQ_FLAGS_NOMULTSEGS|TASK_FEATURE_MULTI_RX,
	.size = sizeof(struct task_nop),
	.mbuf_size = 2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM,
};

static struct task_init task_init_none;

__attribute__((constructor)) static void reg_task_nop(void)
{
	reg_task(&task_init_nop_thrpt_opt);
	reg_task(&task_init_nop_lat_opt);

	/* For backwards compatibility, add none */
	task_init_none = task_init_nop_thrpt_opt;
	strcpy(task_init_none.mode_str, "none");

	reg_task(&task_init_none);
}
