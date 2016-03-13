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

#include "prox_cksum.h"
#include <rte_byteorder.h>

/* compute IP 16 bit checksum */
void prox_ip_cksum_sw(struct ipv4_hdr *buf)
{
	const uint16_t size = sizeof(struct ipv4_hdr);
	uint32_t cksum = 0;
	uint32_t nb_dwords;
	uint32_t tail, mask;
	uint32_t *pdwd = (uint32_t *)buf;

	/* compute 16 bit checksum using hi and low parts of 32 bit integers */
	for (nb_dwords = (size >> 2); nb_dwords > 0; --nb_dwords) {
		cksum += (*pdwd >> 16);
		cksum += (*pdwd & 0xFFFF);
		++pdwd;
	}

	/* deal with the odd byte length */
	if (size & 0x03) {
		tail = *pdwd;
		/* calculate mask for valid parts */
		mask = 0xFFFFFFFF << ((size & 0x03) << 3);
		/* clear unused bits */
		tail &= ~mask;

		cksum += (tail >> 16) + (tail & 0xFFFF);
	}

	cksum = (cksum >> 16) + (cksum & 0xFFFF);
	cksum = (cksum >> 16) + (cksum & 0xFFFF);

	buf->hdr_checksum = ~((uint16_t)cksum);
}

static void prox_write_udp_cksum_pseudo_hdr(struct ipv4_hdr *ip, uint16_t l3_len)
{
	struct udp_hdr *udp = (struct udp_hdr *)((uint8_t *)ip + l3_len);
	uint32_t csum = 0;
	uint32_t src = rte_bswap32(ip->src_addr);
	uint32_t dst = rte_bswap32(ip->dst_addr);
	uint16_t l = rte_bswap16(udp->dgram_len);
	csum += (src >> 16) + (src & 0xFFFF) + (dst >> 16) + (dst & 0xFFFF) + ip->next_proto_id + l;
	csum = (csum >> 16) + (csum & 0xFFFF);
	udp->dgram_cksum =  rte_bswap16((uint16_t)csum);
}

static void prox_write_tcp_cksum_pseudo_hdr(struct ipv4_hdr *ip, uint16_t l3_len)
{
	struct tcp_hdr *tcp = (struct tcp_hdr *)((uint8_t *)ip + l3_len);
	uint32_t csum = 0;
	uint32_t src = rte_bswap32(ip->src_addr);
	uint32_t dst = rte_bswap32(ip->dst_addr);
	uint16_t l = rte_bswap16(ip->total_length) - (ip->version_ihl & 0xF) * 4;
	csum += (src >> 16) + (src & 0xFFFF) + (dst >> 16) + (dst & 0xFFFF) + ip->next_proto_id + l;
	csum = (csum >> 16) + (csum & 0xFFFF);
	tcp->cksum =  rte_bswap16((uint16_t)csum);
}

void prox_ip_udp_cksum(struct rte_mbuf *mbuf, struct ipv4_hdr *pip, uint16_t l2_len, uint16_t l3_len)
{
	prox_ip_cksum(mbuf, pip, l2_len, l3_len);
	mbuf->ol_flags |= PKT_TX_UDP_CKSUM;
	if (pip->next_proto_id == IPPROTO_UDP) {
		prox_write_udp_cksum_pseudo_hdr(pip, l3_len);
	} else if (pip->next_proto_id == IPPROTO_TCP) {
		 prox_write_tcp_cksum_pseudo_hdr(pip, l3_len);
	}
}
