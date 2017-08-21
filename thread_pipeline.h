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

#ifndef _THREAD_PIPELINE_H_
#define _THREAD_PIPELINE_H_

#include <rte_pipeline.h>

#include "lconf.h"
#include "task_base.h"

/* Tasks based on Packet Framework pipelines */
struct task_pipe {
	struct task_base base;

	struct rte_pipeline *p;
	uint32_t port_in_id[MAX_RINGS_PER_TASK];
	uint32_t port_out_id[MAX_RINGS_PER_TASK];
	uint32_t table_id[MAX_RINGS_PER_TASK];
	uint8_t n_ports_in;
	uint8_t n_ports_out;
	uint8_t n_tables;
};

/* Helper function: create pipeline, input ports and output ports */
void init_pipe_create_in_out(struct task_pipe *tpipe, struct task_args *targ);

/* Helper function: connect pipeline input ports to one pipeline table */
void init_pipe_connect_one(struct task_pipe *tpipe, struct task_args *targ, uint32_t table_id);

/* Helper function: connect pipeline input ports to all pipeline tables */
void init_pipe_connect_all(struct task_pipe *tpipe, struct task_args *targ);

/* Helper function: enable pipeline input ports */
void init_pipe_enable(struct task_pipe *tpipe, struct task_args *targ);

/* Helper function: check pipeline consistency */
void init_pipe_check(struct task_pipe *tpipe, struct task_args *targ);

/* This function will panic on purpose: tasks based on Packet Framework
   pipelines should not be invoked via the usual task_base.handle_bulk method */
int handle_pipe(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts);

/* The pipeline thread can only run tasks based on Packet Framework pipelines */
int thread_pipeline(struct lcore_cfg *lconf);

#endif /* _THREAD_PIPELINE_H_ */
