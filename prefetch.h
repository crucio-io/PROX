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

#ifndef _PREFETCH_H_
#define _PREFETCH_H_

#include <rte_mbuf.h>

static inline void prefetch_nta(volatile void *p)
{
	asm volatile ("prefetchnta %[p]" : [p] "+m" (*(volatile char *)p));
}

#ifdef PROX_PREFETCH_OFFSET
#define PREFETCH0(p)		rte_prefetch0(p)
#define PREFETCH_OFFSET		PROX_PREFETCH_OFFSET
#else
#define PREFETCH0(p)		do {} while (0)
#define PREFETCH_OFFSET		0
#endif

static inline void prefetch_pkts(__attribute__((unused)) struct rte_mbuf **mbufs, __attribute__((unused)) uint16_t n_pkts)
{
#ifdef PROX_PREFETCH_OFFSET
	for (uint16_t j = 0; j < PROX_PREFETCH_OFFSET && j < n_pkts; ++j) {
		PREFETCH0(mbufs[j]);
	}
	for (uint16_t j = PROX_PREFETCH_OFFSET; j < n_pkts; ++j) {
		PREFETCH0(mbufs[j]);
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j - PROX_PREFETCH_OFFSET], void*));
	}
	for (uint16_t j = n_pkts - PROX_PREFETCH_OFFSET; j < n_pkts; ++j) {
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j], void*));
	}
#endif
}

static inline void prefetch_first(__attribute__((unused)) struct rte_mbuf **mbufs, __attribute__((unused)) uint16_t n_pkts)
{
#ifdef PROX_PREFETCH_OFFSET
	for (uint16_t j = 0; j < PROX_PREFETCH_OFFSET && j < n_pkts; ++j) {
		PREFETCH0(mbufs[j]);
	}
	for (uint16_t j = 1; j < PROX_PREFETCH_OFFSET && j < n_pkts; ++j) {
		PREFETCH0(rte_pktmbuf_mtod(mbufs[j - 1], void *));
	}
#endif
}

#endif /* _PREFETCH_H_ */
