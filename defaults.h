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

#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_

#include <rte_ether.h>

struct prox_cfg;
struct lcore_cfg;

void set_global_defaults(struct prox_cfg* prox_cfg);
void set_task_defaults(struct prox_cfg* prox_cfg, struct lcore_cfg* lcore_cfg_init);
void set_port_defaults(void);

#define MAX_PKT_BURST   64
#define MAX_RING_BURST	64
#define DUMP_PKT_LEN 128

#if MAX_RING_BURST < MAX_PKT_BURST
#error MAX_RING_BURST < MAX_PKT_BURST
#endif

#define NUM_VCPES               65536
#define GRE_BUCKET_ENTRIES      4
#define MAX_GRE                 (NUM_VCPES * GRE_BUCKET_ENTRIES)
#define MAX_RSS_QUEUE_BITS      9

#define PROX_VLAN_TAG_SIZE	4
#define MBUF_SIZE (ETHER_MAX_LEN + (unsigned)sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM +  2 * PROX_VLAN_TAG_SIZE)

#define PROX_MTU   ETHER_MAX_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN

#endif /* _DEFAULTS_H_ */
