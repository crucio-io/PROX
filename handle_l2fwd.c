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

#include <rte_mbuf.h>

#include "task_init.h"
#include "task_base.h"
#include "lconf.h"
#include "log.h"
#include "prox_port_cfg.h"

struct task_l2fwd {
	struct task_base base;
	uint8_t src_dst_mac[12];
	uint32_t runtime_flags;
};

static int handle_l2fwd_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_l2fwd *task = (struct task_l2fwd *)tbase;
	struct ether_hdr *hdr;
	struct ether_addr mac;

	if ((task->runtime_flags & (TASK_ARG_DST_MAC_SET|TASK_ARG_SRC_MAC_SET)) == (TASK_ARG_DST_MAC_SET|TASK_ARG_SRC_MAC_SET)) {
		/* Source and Destination mac hardcoded */
		for (uint16_t j = 0; j < n_pkts; ++j) {
			hdr = rte_pktmbuf_mtod(mbufs[j], struct ether_hdr *);
               		rte_memcpy(hdr, task->src_dst_mac, sizeof(task->src_dst_mac));
		}
	} else {
		for (uint16_t j = 0; j < n_pkts; ++j) {
			hdr = rte_pktmbuf_mtod(mbufs[j], struct ether_hdr *);
			if ((task->runtime_flags & (TASK_ARG_DO_NOT_SET_SRC_MAC|TASK_ARG_SRC_MAC_SET)) == 0) {
				/* dst mac will be used as src mac */
				ether_addr_copy(&hdr->d_addr, &mac);
			}

			if (task->runtime_flags & TASK_ARG_DST_MAC_SET)
				ether_addr_copy((struct ether_addr *)&task->src_dst_mac[0], &hdr->d_addr);
			else if ((task->runtime_flags & TASK_ARG_DO_NOT_SET_DST_MAC) == 0)
				ether_addr_copy(&hdr->s_addr, &hdr->d_addr);

			if (task->runtime_flags & TASK_ARG_SRC_MAC_SET) {
				ether_addr_copy((struct ether_addr *)&task->src_dst_mac[6], &hdr->s_addr);
			} else if ((task->runtime_flags & TASK_ARG_DO_NOT_SET_SRC_MAC) == 0) {
				ether_addr_copy(&mac, &hdr->s_addr);
			}
		}
	}
	return task->base.tx_pkt(&task->base, mbufs, n_pkts, NULL);
}

static void init_task_l2fwd(struct task_base *tbase, struct task_args *targ)
{
	struct task_l2fwd *task = (struct task_l2fwd *)tbase;
	struct ether_addr *src_addr, *dst_addr;

	/*
	 * Destination MAC can come from
	 *    - pre-configured mac in case 'dst mac=xx:xx:xx:xx:xx:xx' in config file
	 *    - src mac from the packet in case 'dst mac=packet' in config file
	 *    - not written in case 'dst mac=no' in config file
	 *    - (default - no 'dst mac') src mac from the packet
	 * Source MAC can come from
	 *    - pre-configured mac in case 'src mac=xx:xx:xx:xx:xx:xx' in config file
	 *    - dst mac from the packet in case 'src mac=packet' in config file
	 *    - not written in case 'src mac=no' in config file
	 *    - (default - no 'src mac') if (tx_port) port mac
	 *    - (default - no 'src mac') if (no tx_port) dst mac from the packet
	 */

	if (targ->flags & TASK_ARG_DST_MAC_SET) {
		dst_addr = &targ->edaddr;
		memcpy(&task->src_dst_mac[0], dst_addr, sizeof(*src_addr));
	}

	if (targ->flags & TASK_ARG_SRC_MAC_SET) {
		src_addr =  &targ->esaddr;
		memcpy(&task->src_dst_mac[6], src_addr, sizeof(*dst_addr));
		plog_info("\t\tCore %d: src mac set from config file\n", targ->lconf->id);
	} else if ((targ->flags & TASK_ARG_DO_NOT_SET_SRC_MAC) == 0) {
		if (targ->nb_txports) {
			src_addr = &prox_port_cfg[task->base.tx_params_hw.tx_port_queue[0].port].eth_addr;
			targ->flags |= TASK_ARG_SRC_MAC_SET;
			plog_info("\t\tCore %d: src mac set from port\n", targ->lconf->id);
			memcpy(&task->src_dst_mac[6], src_addr, sizeof(*dst_addr));
		}
	}
	task->runtime_flags = targ->flags;
}

static struct task_init task_init_l2fwd = {
	.mode_str = "l2fwd",
	.init = init_task_l2fwd,
	.handle = handle_l2fwd_bulk,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS|TASK_FEATURE_TXQ_FLAGS_NOOFFLOADS|TASK_FEATURE_TXQ_FLAGS_NOMULTSEGS,
	.size = sizeof(struct task_l2fwd),
	.mbuf_size = 2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM,
};

__attribute__((constructor)) static void reg_task_l2fwd(void)
{
	reg_task(&task_init_l2fwd);
}
