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
#include <libgen.h>
#include <rte_sched.h>
#include <rte_version.h>

#include "lconf.h"
#include "defaults.h"
#include "defines.h"
#include "prox_cfg.h"
#include "prox_port_cfg.h"
#include "etypes.h"
#include "toeplitz.h"

#define TEN_GIGABIT     1250000000
#define QUEUE_SIZES     128
#define NB_PIPES        32768
#define NB_MBUF         4096
#define RING_RX_SIZE    256
#define NB_RX_RING_DESC 256
#define NB_TX_RING_DESC 256

/* 1500000 milliseconds */
#define DEFAULT_CPE_TIMEOUT_MS    1500000

/**/
#if DEFAULT_CPE_TIMEOUT_MS < (DRAIN_TIMEOUT/3000000)
#error DEFAULT_CPE_TIMEOUT_MS too small (needs to be at least 2 ms)
#endif

static const struct rte_eth_conf default_port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /* Header Split disabled */
		.hw_ip_checksum = 0, /* IP checksum offload disabled */
		.hw_vlan_filter = 0, /* VLAN filtering disabled */
		.hw_vlan_strip = 0, /* VLAN filtering disabled */
		.jumbo_frame    = 0, /* Jumbo frame support disabled */
		.hw_strip_crc   = 1, /* CRC stripped by hardware --- always set to 1 in VF */
		.hw_vlan_extend = 0,
		.mq_mode        = 0
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
		},
	},
	.intr_conf = {
		.lsc = 1, /* lsc interrupt feature enabled */
	},
};

static const struct rte_eth_rxconf default_rx_conf = {
	.rx_free_thresh = 32,
};

static struct rte_eth_txconf default_tx_conf = {
	.tx_thresh = {
		.pthresh = 32,
		.hthresh = 8,
		.wthresh = 0,
	},
	.tx_free_thresh = 32, /* Use PMD default values */
	.tx_rs_thresh = 32, /* Use PMD default values */
};

static struct rte_sched_port_params port_params_default = {
	.name = "port_0",
	.socket = 0,
	.mtu = 6 + 6 + 4 + 4 + 2 + 1500,
	.rate = 0,
	.frame_overhead = RTE_SCHED_FRAME_OVERHEAD_DEFAULT,
	.n_subports_per_port = 1,
	.n_pipes_per_subport = NB_PIPES,
	.qsize = {QUEUE_SIZES, QUEUE_SIZES, QUEUE_SIZES, QUEUE_SIZES},
	.pipe_profiles = NULL,
	.n_pipe_profiles = 1 /* only one profile */
};

static struct rte_sched_pipe_params pipe_params_default = {
	.tb_rate = TEN_GIGABIT / NB_PIPES,
	.tb_size = 4000000,

	.tc_rate = {TEN_GIGABIT / NB_PIPES, TEN_GIGABIT / NB_PIPES, TEN_GIGABIT / NB_PIPES, TEN_GIGABIT / NB_PIPES},
	.tc_period = 40,

	.wrr_weights = {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1},
};

static struct rte_sched_subport_params subport_params_default = {
	.tb_rate = TEN_GIGABIT,
	.tb_size = 4000000,
	.tc_rate = {TEN_GIGABIT, TEN_GIGABIT, TEN_GIGABIT, TEN_GIGABIT},
	.tc_period = 40, /* default was 10 */
};

void set_global_defaults(__attribute__((unused)) struct prox_cfg *prox_cfg)
{
}

void set_task_defaults(struct prox_cfg* prox_cfg, struct lcore_cfg* lcore_cfg_init)
{
	prox_cfg->master = RTE_MAX_LCORE;

	for (uint32_t i = 0; i < RTE_DIM(prox_cfg->cpe_table_ports); ++i) {
		prox_cfg->cpe_table_ports[i] = -1;
	}

	for (uint8_t lcore_id = 0; lcore_id < RTE_MAX_LCORE; ++lcore_id) {
		struct lcore_cfg *cur_lcore_cfg_init = &lcore_cfg_init[lcore_id];
		cur_lcore_cfg_init->id = lcore_id;
		for (uint8_t task_id = 0; task_id < MAX_TASKS_PER_CORE; ++task_id) {
			struct task_args *targ = &cur_lcore_cfg_init->targs[task_id];
			for (uint8_t port_id = 0; port_id < PROX_MAX_PORTS; ++port_id) {
				targ->rx_ports[port_id] = OUT_DISCARD;
			}
			targ->flags |= TASK_ARG_DROP;
			targ->flags |= TASK_ARG_QINQ_ACL;
			targ->cpe_table_timeout_ms = DEFAULT_CPE_TIMEOUT_MS;
			targ->n_flows = NB_PIPES;
			/* configure default values for QoS (can be overwritten by config) */
			targ->qos_conf.port_params = port_params_default;
			targ->qos_conf.pipe_params[0] = pipe_params_default;
			targ->qos_conf.subport_params[0] = subport_params_default;
			targ->qos_conf.port_params.pipe_profiles = targ->qos_conf.pipe_params;
			targ->qos_conf.port_params.rate = TEN_GIGABIT;
			targ->qinq_tag = ETYPE_8021ad;
			targ->n_concur_conn = 8192*2;

			for (uint8_t port_id = 0; port_id < PROX_MAX_PORTS; ++port_id) {
				targ->tx_port_queue[port_id].port = OUT_DISCARD;
			}

			for (uint8_t i = 0; i < PROX_MAX_PORTS; ++i) {
				targ->mapping[i] = i; // identity
			}

			targ->cbs = ETHER_MAX_LEN;
			targ->ebs = ETHER_MAX_LEN;
			targ->pbs = ETHER_MAX_LEN;

			targ->n_max_rules = 1024;
			targ->ring_size = RING_RX_SIZE;
			targ->nb_cache_mbuf = MAX_PKT_BURST * 4;
			targ->overhead = ETHER_CRC_LEN + 20;

			targ->tunnel_hop_limit = 3;
			targ->ctrl_freq = 1000;
			targ->lb_friend_core = 0xFF;
			targ->mbuf_size = MBUF_SIZE;
			targ->n_pkts = 1024*64;
			targ->runtime_flags |= TASK_TX_CRC;
		}
	}
}

void set_port_defaults(void)
{
	for (uint8_t i = 0; i < PROX_MAX_PORTS; ++i ) {
		prox_port_cfg[i].promiscuous = 1;
		prox_port_cfg[i].n_rxd = NB_RX_RING_DESC;
		prox_port_cfg[i].n_txd = NB_TX_RING_DESC;
		prox_port_cfg[i].port_conf = default_port_conf;
		prox_port_cfg[i].tx_conf = default_tx_conf;
		prox_port_cfg[i].rx_conf = default_rx_conf;
		prox_port_cfg[i].rx_ring[0] = '\0';
		prox_port_cfg[i].tx_ring[0] = '\0';
		prox_port_cfg[i].mtu = PROX_MTU;
	}
}
