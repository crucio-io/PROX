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

#include "task_base.h"
#include "task_init.h"

enum arp_actions {
	UPDATE_FROM_CTRL,
	ARP_REQ_FROM_CTRL,
	ARP_REPLY_FROM_CTRL,
	ARP_TO_CTRL,
	REQ_MAC_TO_CTRL,
	MAX_ACTIONS
};

#define HANDLE_RANDOM_IP_FLAG	1
#define RANDOM_IP		0xffffffff

const char *actions_string[MAX_ACTIONS];

void init_ctrl_plane(struct task_base *tbase);

int (*handle_ctrl_plane)(struct task_base *tbase, struct rte_mbuf **mbuf, uint16_t n_pkts);

static inline void tx_drop(struct rte_mbuf *mbuf)
{
	rte_pktmbuf_free(mbuf);
}

void register_ip_to_ctrl_plane(struct task_base *task, uint32_t ip, uint8_t port_id, uint8_t core_id, uint8_t task_id);
