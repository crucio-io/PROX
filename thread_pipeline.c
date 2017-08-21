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
#include <rte_port_ethdev.h>
#include <rte_port_ring.h>
#include <rte_version.h>

#include "log.h"
#include "quit.h"
#include "thread_pipeline.h"
#include "lconf.h"
#include "defines.h"

/* Helper function: create pipeline, input ports and output ports */
void init_pipe_create_in_out(struct task_pipe *tpipe, struct task_args *targ)
{
	struct task_base *tbase = (struct task_base *)tpipe;
	const char *name = targ->lconf->name;
	const char *mode = targ->task_init->mode_str;
	uint8_t lcore_id = targ->lconf->id;
	uint8_t task_id = targ->task;
	int err;

	/* create pipeline */
	struct rte_pipeline_params pipeline_params = {
		.name = name,
		.socket_id = rte_lcore_to_socket_id(lcore_id),
	};
	tpipe->p = rte_pipeline_create(&pipeline_params);
	PROX_PANIC(tpipe->p == NULL,
			"Failed to create %s pipeline on core %u task %u\n",
			mode, lcore_id, task_id);

	/* create pipeline input ports */
	if (targ->nb_rxrings != 0) {
		for (uint8_t i = 0; i < tbase->rx_params_sw.nb_rxrings; ++i) {
			struct rte_port_ring_reader_params port_ring_params = {
				.ring = tbase->rx_params_sw.rx_rings[i],
			};
			struct rte_pipeline_port_in_params port_params = {
				.ops = &rte_port_ring_reader_ops,
				.arg_create = &port_ring_params,
				.f_action = NULL, //TODO: fill metadata
				.arg_ah = NULL,
				.burst_size = MAX_RING_BURST,
			};
			err = rte_pipeline_port_in_create(tpipe->p,
					&port_params, &tpipe->port_in_id[i]);
			PROX_PANIC(err != 0, "Failed to create SW input port %u "
					"for %s pipeline on core %u task %u: "
					"err = %d\n",
					i, mode, lcore_id, task_id, err);
		}
		tpipe->n_ports_in = tbase->rx_params_sw.nb_rxrings;
	}
	else {
		for (uint8_t i = 0; i < tbase->rx_params_hw.nb_rxports; ++i) {
			struct rte_port_ethdev_reader_params port_ethdev_params = {
				.port_id = tbase->rx_params_hw.rx_pq[i].port,
				.queue_id = tbase->rx_params_hw.rx_pq[i].queue,
			};
			struct rte_pipeline_port_in_params port_params = {
				.ops = &rte_port_ethdev_reader_ops,
				.arg_create = &port_ethdev_params,
				.f_action = NULL, //TODO: fill metadata
				.arg_ah = NULL,
				.burst_size = MAX_PKT_BURST,
			};
			err = rte_pipeline_port_in_create(tpipe->p,
					&port_params, &tpipe->port_in_id[0]);
			PROX_PANIC(err != 0, "Failed to create HW input port "
					"for %s pipeline on core %u task %u: "
					"err = %d\n",
					mode, lcore_id, task_id, err);
		}
		tpipe->n_ports_in = tbase->rx_params_hw.nb_rxports;
	}
	PROX_PANIC(tpipe->n_ports_in < 1, "No input port created "
			"for %s pipeline on core %u task %u\n",
			mode, lcore_id, task_id);

	/* create pipeline output ports */
	if (targ->nb_txrings != 0) {
		for (uint8_t i = 0; i < tbase->tx_params_sw.nb_txrings; ++i) {
			struct rte_port_ring_writer_params port_ring_params = {
				.ring = tbase->tx_params_sw.tx_rings[i],
				.tx_burst_sz = MAX_RING_BURST,
			};
			struct rte_pipeline_port_out_params port_params = {
				.ops = &rte_port_ring_writer_ops,
				.arg_create = &port_ring_params,
				.f_action = NULL,	//TODO
#if RTE_VERSION < RTE_VERSION_NUM(16,4,0,0)
				.f_action_bulk = NULL,	//TODO
#endif
				.arg_ah = NULL,
			};
			err = rte_pipeline_port_out_create(tpipe->p,
					&port_params, &tpipe->port_out_id[i]);
			PROX_PANIC(err != 0, "Failed to create SW output port %u "
					"for %s pipeline on core %u task %u: "
					"err = %d\n",
					i, mode, lcore_id, task_id, err);
		}
		tpipe->n_ports_out = tbase->tx_params_sw.nb_txrings;
	}
	else {
		for (uint8_t i = 0; i < tbase->tx_params_hw.nb_txports; ++i) {
			struct rte_port_ethdev_writer_params port_ethdev_params = {
				.port_id = tbase->tx_params_hw.tx_port_queue[i].port,
				.queue_id = tbase->tx_params_hw.tx_port_queue[i].queue,
				.tx_burst_sz = MAX_PKT_BURST,
			};
			struct rte_pipeline_port_out_params port_params = {
				.ops = &rte_port_ethdev_writer_ops,
				.arg_create = &port_ethdev_params,
				.f_action = NULL,	//TODO
#if RTE_VERSION < RTE_VERSION_NUM(16,4,0,0)
				.f_action_bulk = NULL,	//TODO
#endif
				.arg_ah = NULL,
			};
			err = rte_pipeline_port_out_create(tpipe->p,
					&port_params, &tpipe->port_out_id[i]);
			PROX_PANIC(err != 0, "Failed to create HW output port %u "
					"for %s pipeline on core %u task %u: "
					"err = %d\n",
					i, mode, lcore_id, task_id, err);
		}
		tpipe->n_ports_out = tbase->tx_params_hw.nb_txports;
	}
	PROX_PANIC(tpipe->n_ports_out < 1, "No output port created "
			"for %s pipeline on core %u task %u\n",
			mode, lcore_id, task_id);
}

/* Helper function: connect pipeline input ports to one pipeline table */
void init_pipe_connect_one(struct task_pipe *tpipe, struct task_args *targ,
		uint32_t table_id)
{
	const char *mode = targ->task_init->mode_str;
	uint8_t lcore_id = targ->lconf->id;
	uint8_t task_id = targ->task;
	int err;

	for (uint8_t i = 0; i < tpipe->n_ports_in; ++i) {
		err = rte_pipeline_port_in_connect_to_table(tpipe->p,
				tpipe->port_in_id[i], table_id);
		PROX_PANIC(err != 0, "Failed to connect input port %u to table id %u "
				"for %s pipeline on core %u task %u: "
				"err = %d\n",
				i, table_id, mode, lcore_id, task_id, err);
	}
}

/* Helper function: connect pipeline input ports to all pipeline tables */
void init_pipe_connect_all(struct task_pipe *tpipe, struct task_args *targ)
{
	const char *mode = targ->task_init->mode_str;
	uint8_t lcore_id = targ->lconf->id;
	uint8_t task_id = targ->task;
	int err;

	PROX_PANIC(tpipe->n_tables < tpipe->n_ports_in,
			"Not enough tables (%u) to connect %u input ports "
			"for %s pipeline on core %u task %u\n",
			tpipe->n_tables, tpipe->n_ports_in,
			mode, lcore_id, task_id);

	for (uint8_t i = 0; i < tpipe->n_ports_in; ++i) {
		err = rte_pipeline_port_in_connect_to_table(tpipe->p,
				tpipe->port_in_id[i], tpipe->table_id[i]);
		PROX_PANIC(err != 0, "Failed to connect input port %u to table id %u "
				"for %s pipeline on core %u task %u: "
				"err = %d\n",
				i, tpipe->table_id[i], mode, lcore_id, task_id, err);
	}
}

/* Helper function: enable pipeline input ports */
void init_pipe_enable(struct task_pipe *tpipe, struct task_args *targ)
{
	const char *mode = targ->task_init->mode_str;
	uint8_t lcore_id = targ->lconf->id;
	uint8_t task_id = targ->task;
	int err;

	for (uint8_t i = 0; i < tpipe->n_ports_in; ++i) {
		err = rte_pipeline_port_in_enable(tpipe->p, tpipe->port_in_id[i]);
		PROX_PANIC(err != 0, "Failed to enable input port %u "
				"for %s pipeline on core %u task %u: "
				"err = %d\n",
				i, mode, lcore_id, task_id, err);
	}
}

/* Helper function: check pipeline consistency */
void init_pipe_check(struct task_pipe *tpipe, struct task_args *targ)
{
	const char *mode = targ->task_init->mode_str;
	uint8_t lcore_id = targ->lconf->id;
	uint8_t task_id = targ->task;
	int err;

	err = rte_pipeline_check(tpipe->p);
	PROX_PANIC(err != 0, "Failed consistency check "
			"for %s pipeline on core %u task %u: "
			"err = %d\n",
			mode, lcore_id, task_id, err);
}

/* This function will panic on purpose: tasks based on Packet Framework
   pipelines should not be invoked via the usual task_base.handle_bulk method */
int handle_pipe(struct task_base *tbase,
		__attribute__((unused)) struct rte_mbuf **mbufs,
		__attribute__((unused)) uint16_t n_pkts)
{
	uint32_t lcore_id = rte_lcore_id();
	struct lcore_cfg *lconf = &lcore_cfg[lcore_id];

	for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
		struct task_args *targ = &lconf->targs[task_id];
		if (lconf->tasks_all[task_id] == tbase) {
			PROX_PANIC(1, "Error on core %u task %u: cannot run "
					"%s pipeline and other non-PF tasks\n",
					lcore_id, task_id, targ->task_init->mode_str);
		}
	}
	PROX_PANIC(1, "Error: cannot find task on core %u\n", lcore_id);
	return 0;
}

int thread_pipeline(struct lcore_cfg *lconf)
{
	struct task_pipe *pipes[MAX_TASKS_PER_CORE];
	uint64_t cur_tsc = rte_rdtsc();
	uint64_t term_tsc = cur_tsc + TERM_TIMEOUT;
	uint64_t drain_tsc = cur_tsc + DRAIN_TIMEOUT;
	const uint8_t nb_tasks = lconf->n_tasks_all;

	for (uint8_t task_id = 0; task_id < lconf->n_tasks_all; ++task_id) {
		//TODO: solve other mutually exclusive thread/tasks
		struct task_args *targ = &lconf->targs[task_id];
		PROX_PANIC(targ->task_init->thread_x != thread_pipeline,
				"Invalid task %u '%s' on core %u: %s() can only "
				"run tasks based on Packet Framework pipelines\n",
				targ->task, targ->task_init->mode_str,
				targ->lconf->id, __func__);

		pipes[task_id] = (struct task_pipe *)lconf->tasks_all[task_id];
	}

	lconf->flags |= LCONF_FLAG_RUNNING;
	for (;;) {
		cur_tsc = rte_rdtsc();
		if (cur_tsc > drain_tsc) {
			drain_tsc = cur_tsc + DRAIN_TIMEOUT;

			if (cur_tsc > term_tsc) {
				term_tsc = cur_tsc + TERM_TIMEOUT;
				if (lconf->msg.req && lconf->msg.type == LCONF_MSG_STOP) {
					lconf->flags &= ~LCONF_FLAG_RUNNING;
					break;
				}
				if (!lconf_is_req(lconf)) {
					lconf_unset_req(lconf);
					plog_warn("Command ignored (lconf functions not supported in Packet Framework pipelines)\n");
				}
			}

			for (uint8_t task_id = 0; task_id < nb_tasks; ++task_id) {
				rte_pipeline_flush(pipes[task_id]->p);
			}
		}

		for (uint8_t task_id = 0; task_id < nb_tasks; ++task_id) {
			rte_pipeline_run(pipes[task_id]->p);
		}
	}
	return 0;
}
