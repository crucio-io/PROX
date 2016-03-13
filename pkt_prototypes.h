/*
  Copyright(c) 2010-2015 Intel Corporation.
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

#ifndef _PKT_PROTOTYPES_H_
#define _PKT_PROTOTYPES_H_

#include <rte_ip.h>

#include "gre.h"
#include "qinq.h"
#include "etypes.h"

static const struct gre_hdr gre_hdr_proto = {
	.type = ETYPE_IPv4,
	.version = 0,
	.flags = 0,
	.recur = 0,
	.bits = GRE_KEY_PRESENT
};

static const struct ipv4_hdr tunnel_ip_proto = {
	.version_ihl = 0x45,
	.type_of_service = 0,
	.packet_id = 0,
	.fragment_offset = 0x40,
	/* no fragmentation */
	.time_to_live = 0x40,
	/* gre protocol type */
	.next_proto_id = IPPROTO_GRE,
	.hdr_checksum = 0
};

static const struct qinq_hdr qinq_proto = {
	.svlan.vlan_tci = 0,
	.cvlan.vlan_tci = 0,
	.svlan.eth_proto = ETYPE_8021ad,
	.cvlan.eth_proto = ETYPE_VLAN,
	.ether_type = ETYPE_IPv4
};

#endif /* _PKT_PROTOTYPES_H_ */
