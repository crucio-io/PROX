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

#ifndef _ARP_H_
#define _ARP_H_

#include <rte_ether.h>

#define ARP_REQUEST	0x100
#define ARP_REPLY	0x200

struct _arp_ipv4 {
	struct ether_addr sha; /* Sender hardware address */
	uint32_t spa;          /* Sender protocol address */
	struct ether_addr tha; /* Target hardware address */
	uint32_t tpa;          /* Target protocol address */
} __attribute__((__packed__));
typedef struct _arp_ipv4 arp_ipv4_t;

struct my_arp_t {
	uint16_t   htype;
	uint16_t   ptype;
	uint8_t    hlen;
	uint8_t    plen;
	uint16_t   oper;
	arp_ipv4_t data;
} __attribute__((__packed__));

struct ether_hdr_arp {
	struct ether_hdr ether_hdr;
	struct my_arp_t arp;
};

static int arp_is_gratuitous(struct ether_hdr_arp *hdr)
{
	return hdr->arp.data.spa == hdr->arp.data.tpa;
}

static inline void prepare_arp_reply(struct ether_hdr_arp *hdr_arp, struct ether_addr *s_addr)
{
	uint32_t ip_source = hdr_arp->arp.data.spa;

	hdr_arp->arp.data.spa = hdr_arp->arp.data.tpa;
	hdr_arp->arp.data.tpa = ip_source;
	hdr_arp->arp.oper = 0x200;
	memcpy(&hdr_arp->arp.data.tha, &hdr_arp->arp.data.sha, sizeof(struct ether_addr));
	memcpy(&hdr_arp->arp.data.sha, s_addr, sizeof(struct ether_addr));
}

static void create_mac(struct ether_hdr_arp *hdr, struct ether_addr *addr)
{
        addr->addr_bytes[0] = 0x2;
        addr->addr_bytes[1] = 0;
        // Instead of sending a completely random MAC address, create the following MAC:
        // 02:00:x1:x2:x3:x4 where x1:x2:x3:x4 is the IP address
        memcpy(addr->addr_bytes + 2, (uint32_t *)&hdr->arp.data.tpa, 4);
}

#endif /* _ARP_H_ */
