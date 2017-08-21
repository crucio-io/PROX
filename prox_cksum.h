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

#ifndef _PROX_CKSUM_H_
#define _PROX_CKSUM_H_

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <rte_version.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_mbuf.h>

#if RTE_VERSION >= RTE_VERSION_NUM(1,8,0,0)
#define CALC_TX_OL(l2_len, l3_len) ((uint64_t)(l2_len) | (uint64_t)(l3_len) << 7)
#else
#define CALC_TX_OL(l2_len, l3_len) (((uint64_t)(l2_len) << 9) | (uint64_t)(l3_len))
#endif

static void prox_ip_cksum_hw(struct rte_mbuf *mbuf, uint16_t l2_len, uint16_t l3_len)
{
#if RTE_VERSION < RTE_VERSION_NUM(1,8,0,0)
	mbuf->pkt.vlan_macip.data = CALC_TX_OL(l2_len, l3_len);
#else
	mbuf->tx_offload = CALC_TX_OL(l2_len, l3_len);
#endif
	mbuf->ol_flags |= PKT_TX_IP_CKSUM;
}

void prox_ip_cksum_sw(struct ipv4_hdr *buf);

static inline void prox_ip_cksum(struct rte_mbuf *mbuf, struct ipv4_hdr *buf, uint16_t l2_len, uint16_t l3_len, int offload)
{
	buf->hdr_checksum = 0;
#ifdef SOFT_CRC
	prox_ip_cksum_sw(buf);
#else
	if (offload)
		prox_ip_cksum_hw(mbuf, l2_len, l3_len);
	else {
		prox_ip_cksum_sw(buf);
		/* TODO: calculate UDP checksum */
	}
#endif
}

void prox_ip_udp_cksum(struct rte_mbuf *mbuf, struct ipv4_hdr *buf, uint16_t l2_len, uint16_t l3_len, int cksum_offload);

/* src_ip_addr/dst_ip_addr are in network byte order */
void prox_udp_cksum_sw(struct udp_hdr *udp, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr);
void prox_tcp_cksum_sw(struct tcp_hdr *tcp, uint16_t len, uint32_t src_ip_addr, uint32_t dst_ip_addr);

#endif /* _PROX_CKSUM_H_ */
