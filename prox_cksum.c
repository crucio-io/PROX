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

#include "prox_cksum.h"
#include "prox_port_cfg.h"
#include <rte_byteorder.h>
#include "log.h"

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

static uint16_t calc_pseudo_checksum(uint8_t ipproto, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr)
{
	uint32_t csum = 0;

	csum += (src_ip_addr >> 16) + (src_ip_addr & 0xFFFF);
	csum += (dst_ip_addr >> 16) + (dst_ip_addr & 0xFFFF);
	csum += rte_bswap16(ipproto) + rte_bswap16(len);
	csum = (csum >> 16) + (csum & 0xFFFF);
	return csum;
}

static void prox_write_udp_pseudo_hdr(struct udp_hdr *udp, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr)
{
	/* Note that the csum is not complemented, while the pseaudo
	   header checksum is calculated as "... the 16-bit one's
	   complement of the one's complement sum of a pseudo header
	   of information ...", the psuedoheader forms as a basis for
	   the actual checksum calculated later either in software or
	   hardware. */
	udp->dgram_cksum = calc_pseudo_checksum(IPPROTO_UDP, len, src_ip_addr, dst_ip_addr);
}

static void prox_write_tcp_pseudo_hdr(struct tcp_hdr *tcp, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr)
{
	tcp->cksum = calc_pseudo_checksum(IPPROTO_TCP, len, src_ip_addr, dst_ip_addr);
}

void prox_ip_udp_cksum(struct rte_mbuf *mbuf, struct ipv4_hdr *pip, uint16_t l2_len, uint16_t l3_len, int cksum_offload)
{
	prox_ip_cksum(mbuf, pip, l2_len, l3_len, cksum_offload & IPV4_CKSUM);

	uint32_t l4_len = rte_bswap16(pip->total_length) - l3_len;
	if (pip->next_proto_id == IPPROTO_UDP) {
		struct udp_hdr *udp = (struct udp_hdr *)(((uint8_t*)pip) + l3_len);
#ifndef SOFT_CRC
		if (cksum_offload & UDP_CKSUM) {
			mbuf->ol_flags |= PKT_TX_UDP_CKSUM;
			prox_write_udp_pseudo_hdr(udp, l4_len, pip->src_addr, pip->dst_addr);
		} else
#endif
		prox_udp_cksum_sw(udp, l4_len, pip->src_addr, pip->dst_addr);
	} else if (pip->next_proto_id == IPPROTO_TCP) {
		struct tcp_hdr *tcp = (struct tcp_hdr *)(((uint8_t*)pip) + l3_len);
#ifndef SOFT_CRC
		if (cksum_offload & UDP_CKSUM) {
			prox_write_tcp_pseudo_hdr(tcp, l4_len, pip->src_addr, pip->dst_addr);
			mbuf->ol_flags |= PKT_TX_UDP_CKSUM;
		} else
#endif
		prox_tcp_cksum_sw(tcp, l4_len, pip->src_addr, pip->dst_addr);
	}
}

static uint16_t checksum_byte_seq(uint16_t *buf, uint16_t len)
{
	uint32_t csum = 0;

	while (len > 1) {
		csum += *buf;
		while (csum >> 16) {
			csum &= 0xffff;
			csum +=1;
		}
		buf++;
		len -= 2;
	}

	if (len) {
		csum += *(uint8_t*)buf;
		while (csum >> 16) {
			csum &= 0xffff;
			csum +=1;
		}
	}
	return ~csum;
}

void prox_udp_cksum_sw(struct udp_hdr *udp, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr)
{
	prox_write_udp_pseudo_hdr(udp, len, src_ip_addr, dst_ip_addr);
	uint16_t csum = checksum_byte_seq((uint16_t *)udp, len);
	udp->dgram_cksum = csum;
}

void prox_tcp_cksum_sw(struct tcp_hdr *tcp, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr)
{
	prox_write_tcp_pseudo_hdr(tcp, len, src_ip_addr, dst_ip_addr);

	uint16_t csum = checksum_byte_seq((uint16_t *)tcp, len);
	tcp->cksum = csum;
}
