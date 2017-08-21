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

#ifndef _DEFINES_H_
#define _DEFINES_H_

// with 3GHz CPU
#define DRAIN_TIMEOUT  __UINT64_C(6000000)             // drain TX buffer every 2ms
#define TERM_TIMEOUT   __UINT64_C(3000000000)          // check if terminated every 1s

/* DRAIN_TIMEOUT should be smaller than TERM_TIMEOUT as TERM_TIMEOUT
   is only checked after DRAIN_TIMEOUT */
#if TERM_TIMEOUT < DRAIN_TIMEOUT
#error TERM_TIMEOUT < DRAIN_TIMEOUT
#endif

#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT  "%d.%d.%d.%d"
#define IPv4_BYTES(addr)                        \
        addr[0],  addr[1],  addr[2],  addr[3]
#endif

#ifndef IPv6_BYTES
#define IPv6_BYTES_FMT	"%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define IPv6_BYTES(addr)			\
	addr[0],  addr[1],  addr[2],  addr[3],	\
	addr[4],  addr[5],  addr[6],  addr[7],	\
	addr[8],  addr[9],  addr[10], addr[11],	\
	addr[12], addr[13], addr[14], addr[15]
#endif

#ifndef MAC_BYTES
#define MAC_BYTES_FMT "%02x:%02x:%02x:%02x:%02x:%02x"

#define MAC_BYTES(addr)   \
	addr[0], addr[1], \
	addr[2], addr[3], \
	addr[4], addr[5]
#endif

/* assume cpu byte order is little endian */
#define PKT_TO_LUTQINQ(svlan, cvlan) ((((uint32_t)svlan) & 0x000F) << 4 | (((uint32_t)svlan) & 0xFF00) << 8 | (((uint32_t)cvlan) & 0xFF0F))

#define ROUTE_ERR 254

#endif /* _DEFINES_H_ */
