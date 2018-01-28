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

#ifndef _HASH_UTILS_H_
#define _HASH_UTILS_H_

#include <rte_common.h>
#include <rte_version.h>

struct rte_table_hash;

/* Take DPDK 2.2.0 ABI change into account: offset 0 now means first byte of mbuf struct
 * see http://www.dpdk.org/browse/dpdk/commit/?id=ba92d511ddacf863fafaaa14c0577f30ee57d092
 */
#if RTE_VERSION >= RTE_VERSION_NUM(2,2,0,0)
#define HASH_METADATA_OFFSET(offset)	(sizeof(struct rte_mbuf) + (offset))
#else
#define HASH_METADATA_OFFSET(offset)	(offset)
#endif

/* Wrap crc32 hash function to match that required for rte_table */
uint64_t hash_crc32(void* key, uint32_t key_size, uint64_t seed);

void print_hash_table_size(const struct rte_table_hash *h);
void print_hash_table(const struct rte_table_hash *h);

uint64_t get_bucket_key8(void* table, uint32_t bucket_idx, void** key, void** entries);
uint64_t get_bucket(void* table, uint32_t bucket_idx, void** key, void** entries);
#endif /* _HASH_UTILS_H_ */
