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

#ifndef _HASH_ENTRY_TYPES_H_
#define _HASH_ENTRY_TYPES_H_

#include <rte_ether.h>

struct ether_addr_port {
	struct ether_addr 	mac;
	uint8_t                 pad;
	uint8_t	                out_idx;
};

struct next_hop {
	uint32_t                ip_dst;
	uint32_t                mpls;
	union {
		uint64_t               mac_port_8bytes;
		struct ether_addr_port mac_port;
	};
};

struct next_hop6 {
	uint8_t                ip_dst[16];
	uint32_t               mpls;
	union {
		uint64_t               mac_port_8bytes;
		struct ether_addr_port mac_port;
	};
};

struct cpe_data {
	uint16_t qinq_svlan;
	uint16_t qinq_cvlan;
	uint32_t user;
	union {
		uint64_t               mac_port_8bytes;
		struct ether_addr_port mac_port;
		uint8_t                mac_port_b[8];
	};
	uint64_t tsc;
};

struct cpe_key {
	union {
		uint32_t ip;
		uint8_t ip_bytes[4];
	};
	uint32_t gre_id;
} __attribute__((__packed__));

struct qinq_gre_data {
	uint32_t gre_id;
	uint32_t user;
} __attribute__((__packed__));

#endif /* _HASH_ENTRY_TYPES_H_ */
