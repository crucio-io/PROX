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

#include "arp.h"
#include "quit.h"
#include "prox_malloc.h"
#include "defaults.h"
#include "prox_cfg.h"
#include "etypes.h"

#define FLAG_DST_MAC_KNOWN	1
#define MAX_ARP_ENTRIES	65536

struct task_base;
struct task_args;
struct arp_table {
	uint64_t arp_update_time;
	uint64_t arp_timeout;
	uint32_t ip;
	struct ether_addr mac;
};
struct l3_base {
	struct rte_ring *ctrl_plane_ring;
	struct task_base *tmaster;
	uint32_t flags;
	uint32_t n_pkts;
	uint8_t reachable_port_id;
	uint8_t core_id;
	uint8_t task_id;
	struct arp_table gw;
	struct arp_table optimized_arp_table[4];
	struct rte_hash *ip_hash;
	struct arp_table *arp_table;
};

void task_init_l3(struct task_base *tbase, struct task_args *targ);
void task_start_l3(struct task_base *tbase, struct task_args *targ);
int write_dst_mac(struct task_base *tbase, struct rte_mbuf *mbuf, uint32_t *ip_dst);
void task_set_gateway_ip(struct task_base *tbase, uint32_t ip);
void task_set_local_ip(struct task_base *tbase, uint32_t ip);
void handle_ctrl_plane_pkts(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts);
