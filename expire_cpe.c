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

#include <rte_cycles.h>

#include "hash_entry_types.h"
#include "hash_utils.h"
#include "expire_cpe.h"

#define MAX_TSC	       __UINT64_C(0xFFFFFFFFFFFFFFFF)

void check_expire_cpe(void* data)
{
	struct expire_cpe *um = (struct expire_cpe *)data;
	uint64_t cur_tsc = rte_rdtsc();
	struct cpe_data *entries[4] = {0};
	void *key[4] = {0};
	uint64_t n_buckets = get_bucket_key8(um->cpe_table, um->bucket_index, key, (void**)entries);

	for (uint8_t i = 0; i < 4 && entries[i]; ++i) {
		if (entries[i]->tsc < cur_tsc) {
			int key_found = 0;
			void* entry = 0;
			rte_table_hash_key8_ext_dosig_ops.f_delete(um->cpe_table, key[i], &key_found, entry);
		}
	}

        um->bucket_index++;
        um->bucket_index &= (n_buckets - 1);
}
