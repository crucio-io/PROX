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

#ifndef _BNG_PKTS_H_
#define _BNG_PKTS_H_

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_byteorder.h>

#include "gre.h"
#include "mpls.h"
#include "qinq.h"
#include "arp.h"
#include "hash_entry_types.h"

struct cpe_pkt {
#ifdef USE_QINQ
	struct qinq_hdr qinq_hdr;
#else
	struct ether_hdr ether_hdr;
#endif
	struct ipv4_hdr ipv4_hdr;
	struct udp_hdr udp_hdr;
} __attribute__((packed));

struct cpe_packet_arp {
	struct qinq_hdr qinq_hdr;
	struct my_arp_t arp;
} __attribute__((packed));

/* Struct used for setting all the values a packet
   going to the core netwerk. Payload may follow
   after the headers, but no need to touch that. */
struct core_net_pkt_m {
	struct ether_hdr ether_hdr;
#ifdef MPLS_ROUTING
	union {
		struct mpls_hdr mpls;
		uint32_t mpls_bytes;
	};
#endif
	struct ipv4_hdr tunnel_ip_hdr;
	struct gre_hdr gre_hdr;
	struct ipv4_hdr ip_hdr;
	struct udp_hdr udp_hdr;
} __attribute__((packed));

struct core_net_pkt {
	struct ether_hdr ether_hdr;
	struct ipv4_hdr tunnel_ip_hdr;
	struct gre_hdr gre_hdr;
	struct ipv4_hdr ip_hdr;
	struct udp_hdr udp_hdr;
} __attribute__((packed));

#define UPSTREAM_DELTA   ((uint32_t)(sizeof(struct core_net_pkt) - sizeof(struct cpe_pkt)))
#define DOWNSTREAM_DELTA ((uint32_t)(sizeof(struct core_net_pkt_m) - sizeof(struct cpe_pkt)))

struct cpe_pkt_delta {
	uint8_t encap[DOWNSTREAM_DELTA];
	struct cpe_pkt pkt;
} __attribute__((packed));

static inline void extract_key_cpe(struct rte_mbuf *mbuf, uint64_t* key)
{
	uint8_t* packet = rte_pktmbuf_mtod(mbuf, uint8_t*);
#ifdef USE_QINQ
	*key = (*(uint64_t *)(packet + 12)) & 0xFF0FFFFFFF0FFFFF;
#else
	*key = rte_bswap32(*(uint32_t *)(packet + 26)) & 0x00FFFFFF;
#endif
}

static inline void key_core(struct gre_hdr* gre, __attribute__((unused)) struct ipv4_hdr* ip, uint64_t* key)
{
	struct cpe_key *cpe_key = (struct cpe_key*)key;

	cpe_key->gre_id = rte_be_to_cpu_32(gre->gre_id) & 0xFFFFFFF;

#ifdef USE_QINQ
	cpe_key->ip = ip->dst_addr;
#else
	cpe_key->ip = 0;
#endif
}

static inline void extract_key_core(struct rte_mbuf *mbuf, uint64_t* key)
{
	struct core_net_pkt *packet = rte_pktmbuf_mtod(mbuf, struct core_net_pkt *);
	key_core(&packet->gre_hdr, &packet->ip_hdr, key);
}

static inline void extract_key_core_m(struct rte_mbuf *mbuf, uint64_t* key)
{
	struct core_net_pkt_m *packet = rte_pktmbuf_mtod(mbuf, struct core_net_pkt_m *);
	key_core(&packet->gre_hdr, &packet->ip_hdr, key);
}

#endif /* _BNG_PKTS_H_ */
