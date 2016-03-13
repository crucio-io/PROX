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

#ifndef _FQUEUE_H_
#define _FQUEUE_H_

#include <rte_mbuf.h>

#include <inttypes.h>

struct fqueue {
	uint32_t prod;
	uint32_t cons;
	uint32_t mask;
	struct rte_mbuf *entries[0];
};

static uint32_t fqueue_put(struct fqueue *q, struct rte_mbuf **mbufs, uint32_t count)
{
	uint32_t free_entries = q->mask + q->cons - q->prod;
	uint32_t beg = q->prod & q->mask;

	count = count > free_entries? free_entries : count;

	if ((q->prod & q->mask) + count <= q->mask) {
		rte_memcpy(&q->entries[q->prod & q->mask], mbufs, sizeof(mbufs[0]) * count);
		q->prod += count;
	}
	else {
		for (uint32_t i = 0; i < count; ++i) {
			q->entries[q->prod & q->mask] = mbufs[i];
			q->prod++;
		}
	}
	return count;
}

static uint32_t fqueue_get(struct fqueue *q, struct rte_mbuf **mbufs, uint32_t count)
{
	uint32_t entries = q->prod - q->cons;

	count = count > entries? entries : count;

	if ((q->cons & q->mask) + count <= q->mask) {
		rte_memcpy(mbufs, &q->entries[q->cons & q->mask], sizeof(mbufs[0]) * count);
		q->cons += count;
	}
	else {
         	for (uint32_t i = 0; i < count; ++i) {
			mbufs[i] = q->entries[q->cons & q->mask];
			q->cons++;
		}
	}
	return count;
}

static struct fqueue *fqueue_create(uint32_t size, int socket)
{
	size_t mem_size = 0;

	mem_size += sizeof(struct fqueue);
	mem_size += sizeof(((struct fqueue *)(0))->entries[0]) * size;

	struct fqueue *ret = prox_zmalloc(mem_size, socket);

	if (!ret)
		return NULL;

	ret->mask = size - 1;
	return ret;
}

#endif /* _FQUEUE_H_ */
