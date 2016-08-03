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

#include <string.h>
#include <stdio.h>

#include "prox_cfg.h"

#define CM_N_BITS (sizeof(prox_cfg.core_mask[0]) * 8)
#define CM_ALL_N_BITS (sizeof(prox_cfg.core_mask) * 8)

struct prox_cfg prox_cfg = {
	.update_interval_str = "1"
};

static int prox_cm_isset(const uint32_t lcore_id)
{
	uint64_t cm;
	uint32_t cm_idx;

	if (lcore_id > CM_ALL_N_BITS)
		return -1;

	cm = __UINT64_C(1) << (lcore_id % CM_N_BITS);
	cm_idx = PROX_CM_DIM - 1 - lcore_id / CM_N_BITS;
	return !!(prox_cfg.core_mask[cm_idx] & cm);
}

int prox_core_active(const uint32_t lcore_id, const int with_master)
{
	int ret;

	ret = prox_cm_isset(lcore_id);
	if (ret < 0)
		return 0;

	if (with_master)
		return ret || lcore_id == prox_cfg.master;
	else
		return ret && lcore_id != prox_cfg.master;
}

int prox_core_next(uint32_t* lcore_id, const int with_master)
{
	for (uint32_t i = *lcore_id + 1; i < CM_ALL_N_BITS; ++i) {
		if (prox_core_active(i, with_master)) {
			*lcore_id = i;
			return 0;
		}
	}
	return -1;
}

int prox_core_to_hex(char *dst, const size_t size, const int with_master)
{
	uint64_t cm;
	uint32_t cm_len;
	uint32_t cm_first = 0;
	uint32_t master = prox_cfg.master;

	/* Minimum size of the string has to big enough to hold the
	   bitmask in hex (including the prefix "0x"). */
	if (size < PROX_CM_STR_LEN)
		return 0;

	snprintf(dst, size, "0x");
	for (uint32_t i = 0; i < PROX_CM_DIM; ++i, cm_first = i) {
		if ((with_master && ((CM_ALL_N_BITS - 1 - master) / CM_N_BITS == i * CM_N_BITS)) ||
		    prox_cfg.core_mask[i]) {
			break;
		}
	}

	for (uint32_t i = cm_first; i < PROX_CM_DIM; ++i) {
		cm = prox_cfg.core_mask[i];
		if (with_master && ((CM_ALL_N_BITS - 1 - master) / CM_N_BITS == i)) {
			cm |= (__UINT64_C(1) << (master % CM_N_BITS));
		}

		snprintf(dst + strlen(dst), size - strlen(dst), i == cm_first? "%lx" : "%016lx", cm);
	}

	return 0;
}

int prox_core_to_str(char *dst, const size_t size, const int with_master)
{
	uint32_t lcore_id = -1;
	uint32_t first = 1;

	*dst = 0;
	lcore_id - 1;
	while (prox_core_next(&lcore_id, with_master) == 0) {
		/* Stop printing to string if there is not engough
		   space left. Assume that adding 1 core to the string
		   will take at most 5 + 1 bytes implying that
		   lcore_id < 999. Check if ther is space for another
		   6 bytes to add an elipsis */
		if (12 + strlen(dst) > size) {
			if (6 + strlen(dst) > size) {
				snprintf(dst + strlen(dst), size - strlen(dst), ", ...");
				return 0;
			}
			return -1;
		}

		snprintf(dst + strlen(dst), size - strlen(dst), first? "%u" : ", %u", lcore_id);
		first = 0;
	}

	return 0;
}

void prox_core_clr(void)
{
	memset(prox_cfg.core_mask, 0, sizeof(prox_cfg.core_mask));
}

int prox_core_set_active(const uint32_t lcore_id)
{
	uint32_t cm_idx;
	uint64_t cm;

	if (lcore_id > CM_ALL_N_BITS)
		return -1;

	cm = __UINT64_C(1) << (lcore_id % CM_N_BITS);
	cm_idx = PROX_CM_DIM - 1 - lcore_id / CM_N_BITS;
	prox_cfg.core_mask[cm_idx] |= cm;

	return 0;
}
