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

#include <string.h>
#include <rte_hash_crc.h>
#include <rte_table_hash.h>
#include <rte_version.h>

#include "hash_utils.h"

/* These opaque structure definitions were copied from DPDK lib/librte_table/rte_table_hash_key8.c */

struct rte_bucket_4_8 {
	/* Cache line 0 */
	uint64_t signature;
	uint64_t lru_list;
	struct rte_bucket_4_8 *next;
	uint64_t next_valid;

	uint64_t key[4];

	/* Cache line 1 */
	uint8_t data[0];
};

struct rte_table_hash_key8 {
#if RTE_VERSION >= RTE_VERSION_NUM(2,1,0,0)
	struct rte_table_stats stats;
#endif
	/* Input parameters */
	uint32_t n_buckets;
	uint32_t n_entries_per_bucket;
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t bucket_size;
	uint32_t signature_offset;
	uint32_t key_offset;
#if RTE_VERSION >= RTE_VERSION_NUM(2,2,0,0)
	uint64_t key_mask;
#endif
	rte_table_hash_op_hash f_hash;
	uint64_t seed;

	/* Extendible buckets */
	uint32_t n_buckets_ext;
	uint32_t stack_pos;
	uint32_t *stack;

	/* Lookup table */
	uint8_t memory[0] __rte_cache_aligned;
};

/* These opaque structure definitions were copied from DPDK lib/librte_table/rte_table_hash_ext.c */

struct bucket {
	union {
		uintptr_t next;
		uint64_t lru_list;
	};
	uint16_t sig[4];
	uint32_t key_pos[4];
};

#define BUCKET_NEXT(bucket)						\
	((void *) ((bucket)->next & (~1LU)))

struct grinder {
	struct bucket *bkt;
	uint64_t sig;
	uint64_t match;
	uint32_t key_index;
};

struct rte_table_hash_ext {
#if RTE_VERSION >= RTE_VERSION_NUM(2,1,0,0)
	struct rte_table_stats stats;
#endif
	/* Input parameters */
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t n_keys;
	uint32_t n_buckets;
	uint32_t n_buckets_ext;
	rte_table_hash_op_hash f_hash;
	uint64_t seed;
	uint32_t signature_offset;
	uint32_t key_offset;

	/* Internal */
	uint64_t bucket_mask;
	uint32_t key_size_shl;
	uint32_t data_size_shl;
	uint32_t key_stack_tos;
	uint32_t bkt_ext_stack_tos;

	/* Grinder */
	struct grinder grinders[64];

	/* Tables */
	struct bucket *buckets;
	struct bucket *buckets_ext;
	uint8_t *key_mem;
	uint8_t *data_mem;
	uint32_t *key_stack;
	uint32_t *bkt_ext_stack;

	/* Table memory */
	uint8_t memory[0] __rte_cache_aligned;
};

uint64_t get_bucket(void* table, uint32_t bucket_idx, void** key, void** entries)
{
	struct rte_table_hash_ext *t = (struct rte_table_hash_ext *) table;
	struct bucket *bkt0, *bkt, *bkt_prev;
	uint64_t sig;
	uint32_t bkt_index, i;
	uint8_t n = 0;
	bkt_index = bucket_idx & t->bucket_mask;
	bkt0 = &t->buckets[bkt_index];
	sig = (bucket_idx >> 16) | 1LLU;

	/* Key is present in the bucket */
	for (bkt = bkt0; bkt != NULL; bkt = BUCKET_NEXT(bkt)) {
		for (i = 0; i < 4; i++) {
			uint64_t bkt_sig = (uint64_t) bkt->sig[i];
			uint32_t bkt_key_index = bkt->key_pos[i];
			uint8_t *bkt_key =
				&t->key_mem[bkt_key_index << t->key_size_shl];

			if (sig == bkt_sig) {
				key[n] = bkt_key;
				entries[n++] = &t->data_mem[bkt_key_index << t->data_size_shl];
				/* Assume no more than 4 entries in total (including extended state) */
				if (n == 4)
					return t->n_buckets;
			}
		}
	}
	return t->n_buckets;
}

uint64_t get_bucket_key8(void* table, uint32_t bucket_idx, void** key, void** entries)
{
	struct rte_bucket_4_8 *bucket, *bucket0;
	struct rte_table_hash_key8* f = table;
	uint8_t n = 0;

	bucket0 = (struct rte_bucket_4_8 *) &f->memory[bucket_idx * f->bucket_size];
	for (bucket = bucket0; bucket != NULL; bucket = bucket->next) {
		uint64_t mask;

		for (uint8_t i = 0, mask = 1LLU; i < 4; i++, mask <<= 1) {
			uint64_t bucket_signature = bucket->signature;

			if (bucket_signature & mask) {
				key[n] = &bucket->key[i];
				entries[n++] = &bucket->data[i *f->entry_size];
				/* Assume no more than 4 entries
				   in total (including extended state) */
				if (n == 4)
					return f->n_buckets;
			}
		}
	}
	return f->n_buckets;
}

uint64_t hash_crc32(void* key, uint32_t key_size, uint64_t seed)
{
	return rte_hash_crc(key, key_size, seed);
}
