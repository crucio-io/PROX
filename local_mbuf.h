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

#ifndef _LOCAL_MBUF_H_
#define _LOCAL_MBUF_H_
#define LOCAL_MBUF_COUNT 64

struct local_mbuf {
	struct rte_mempool *mempool;
	uint32_t           n_new_pkts;
	struct rte_mbuf    *new_pkts[LOCAL_MBUF_COUNT];
};

static struct rte_mbuf **local_mbuf_take(struct local_mbuf *local_mbuf, uint32_t count)
{
	PROX_ASSERT(local_mbuf->n_new_pkts >= count);

	const uint32_t start_pos = local_mbuf->n_new_pkts - count;
	struct rte_mbuf **ret = &local_mbuf->new_pkts[start_pos];

	local_mbuf->n_new_pkts -= count;
	return ret;
}

static int local_mbuf_refill(struct local_mbuf *local_mbuf)
{
	const uint32_t fill = LOCAL_MBUF_COUNT - local_mbuf->n_new_pkts;
	struct rte_mbuf **fill_mbuf = &local_mbuf->new_pkts[local_mbuf->n_new_pkts];

	if (rte_mempool_get_bulk(local_mbuf->mempool, (void **)fill_mbuf, fill) < 0)
		return -1;
	local_mbuf->n_new_pkts += fill;
	return 0;
}

/* Ensures that count or more mbufs are available. Returns pointer to
   count allocated mbufs or NULL if not enough mbufs are available. */
static struct rte_mbuf **local_mbuf_refill_and_take(struct local_mbuf *local_mbuf, uint32_t count)
{
	PROX_ASSERT(count <= LOCAL_MBUF_COUNT);
	if (local_mbuf->n_new_pkts >= count)
		return local_mbuf_take(local_mbuf, count);

	if (local_mbuf_refill(local_mbuf) == 0)
		return local_mbuf_take(local_mbuf, count);
	return NULL;
}

#endif /* _LOCAL_MBUF_H_ */
