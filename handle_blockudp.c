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
#include <rte_ip.h>
#include <rte_ether.h>

#include "task_base.h"
#include "task_init.h"
#include "defines.h"
#include "etypes.h"
#include "prefetch.h"
#include "log.h"

struct task_blockudp {
	struct task_base    base;
};

static int handle_blockudp_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct task_blockudp *task = (struct task_blockudp *)tbase;
	uint8_t out[MAX_PKT_BURST];
	uint16_t j;

	for (j = 0; j < n_pkts; ++j) {
		struct ether_hdr *peth = rte_pktmbuf_mtod(mbufs[j], struct ether_hdr *);
		struct ipv4_hdr *pip = (struct ipv4_hdr *) (peth + 1);
		out[j] = peth->ether_type == ETYPE_IPv4 && pip->next_proto_id == 0x11 ? OUT_DISCARD : 0;
	}

	return task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static void init_task_blockudp(__attribute__((unused)) struct task_base *tbase,
			   __attribute__((unused)) struct task_args *targ)
{
}

static struct task_init task_init_blockudp = {
	.mode_str = "blockudp",
	.init = init_task_blockudp,
	.handle = handle_blockudp_bulk,
	.size = sizeof(struct task_blockudp)
};

__attribute__((constructor)) static void reg_task_blockudp(void)
{
	reg_task(&task_init_blockudp);
}
