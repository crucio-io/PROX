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

#include <stdlib.h>
#include <inttypes.h>

#include <rte_cycles.h>

#include "prox_malloc.h"
#include "cdf.h"

#ifndef RTE_CACHE_LINE_SIZE
#define RTE_CACHE_LINE_SIZE CACHE_LINE_SIZE
#endif

static uint32_t round_pow2(uint32_t val)
{
	uint32_t ret;
	uint32_t s = 1 << 31;

	while ((s & val) == 0)
		s = s >> 1;
	if (s == 1U << 31 && s != val)
		return 0;

	ret = val;
	if (s != ret)
		ret = (s << 1);

	return ret;
}

static uint32_t get_r_max(struct cdf *cdf, uint32_t cur)
{
	uint32_t right_child = cur;

	do {
		cur = right_child;
		right_child = cur * 2 + 1;
	} while (right_child < cdf->elems[0]);

	return cdf->elems[cur];
}

struct cdf *cdf_create(uint32_t n_vals, int socket_id)
{
	struct cdf *ret;
	size_t mem_size = 0;
	uint32_t n_vals_round = round_pow2(n_vals);

	if (0 == n_vals_round)
		return NULL;

	mem_size += sizeof(struct cdf);
	mem_size += sizeof(((struct cdf *)(0))->elems[0]) * n_vals_round * 2;
	ret = prox_zmalloc(mem_size, socket_id);
	ret->elems[0] = n_vals;

	/* leafs are [n_vals, 2 * n_vals[. During cdf_add() and
	   cdf_setup(), rand_max refers to the index of the next leaf
	   to be added.  */
	ret->rand_max = n_vals_round;
	ret->first_child = n_vals_round;
	ret->seed = rte_rdtsc();

	return ret;
}

void cdf_add(struct cdf *cdf, uint32_t len)
{
	cdf->elems[cdf->rand_max++] = len;
}

int cdf_setup(struct cdf *cdf)
{
	uint32_t last_leaf, first_leaf;
	uint32_t first_parent, last_parent;
	uint32_t total, multiplier, cur, end;

	if (cdf->elems[0] == 1) {
		cdf->rand_max = RAND_MAX;
		cdf->elems[1] = RAND_MAX;
		cdf->elems[0] = 2;
		return 0;
	}

	last_leaf  = cdf->rand_max;
	first_leaf = round_pow2(cdf->elems[0]);
	/* Failed to add all elements through cdf_add() */
	if (last_leaf - first_leaf != cdf->elems[0])
		return -1;

	total = 0;
	for (uint32_t i = first_leaf; i < last_leaf; ++i) {
		total += cdf->elems[i];
	}

	multiplier = RAND_MAX / total;
	if (multiplier * total == RAND_MAX)
		multiplier--;
	cdf->rand_max = multiplier * total;
	total = 0;
	for (uint32_t i = first_leaf; i < last_leaf; ++i) {
		uint32_t cur = cdf->elems[i];

		/* Each element represents the range between previous
		   total (non-inclusive) and new total (inclusive). */
		total += cur * multiplier - 1;
		cdf->elems[i] = total;
		total += 1;
	}
	end = round_pow2(first_leaf) << 1;
	for (uint32_t i = last_leaf; i < end; ++i) {
		cdf->elems[i] = RAND_MAX;
	}
	cdf->first_child = first_leaf;
	cdf->elems[0] = end;

	/* Build the binary tree used at run-time. */
	last_leaf = end - 1;
	do {
		first_parent = first_leaf/2;
		last_parent  = last_leaf/2;

		for (uint32_t i = first_parent; i <= last_parent; ++i) {
			/* The current nodes value should be the
			   biggest value accessible through its left
			   child. This value is stored in the right
			   most child of the left child. The left most
			   child of the right child is the first value
			   that can not be accessed through the left
			   child.  */
			cdf->elems[i] = get_r_max(cdf, i * 2);
		}
		first_leaf = first_parent;
		last_leaf = last_parent;
	} while (first_parent != last_parent);
	return 0;
}
