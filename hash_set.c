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

#include <rte_hash_crc.h>
#include <string.h>

#include "prox_malloc.h"
#include "prox_assert.h"
#include "hash_set.h"

#define HASH_SET_ALLOC_CHUNCK 1024
#define HASH_SET_ALLOC_CHUNCK_MEM (sizeof(struct hash_set_entry) * 1024)

struct hash_set_entry {
	uint32_t              crc;
	void                  *data;
	size_t                len;
	struct hash_set_entry *next;
};

struct hash_set {
	uint32_t              n_buckets;
	int                   socket_id;
	struct hash_set_entry *alloc;
	size_t                alloc_count;
	struct hash_set_entry *mem[0];
};

static struct hash_set_entry *hash_set_alloc_entry(struct hash_set *hs)
{
	struct hash_set_entry *ret;

	if (hs->alloc_count == 0) {
		size_t mem_size = HASH_SET_ALLOC_CHUNCK *
			sizeof(struct hash_set_entry);

		hs->alloc = prox_zmalloc(mem_size, hs->socket_id);
		hs->alloc_count = HASH_SET_ALLOC_CHUNCK;
	}

	ret = hs->alloc;
	hs->alloc++;
	hs->alloc_count--;
	return ret;
}

struct hash_set *hash_set_create(uint32_t n_buckets, int socket_id)
{
	struct hash_set *ret;
	size_t mem_size = sizeof(*ret) + sizeof(ret->mem[0]) * n_buckets;

	ret = prox_zmalloc(mem_size, socket_id);
	ret->n_buckets = n_buckets;
	ret->socket_id = socket_id;

	return ret;
}

void *hash_set_find(struct hash_set *hs, void *data, size_t len)
{
	uint32_t crc = rte_hash_crc(data, len, 0);

	struct hash_set_entry *entry = hs->mem[crc % hs->n_buckets];

	while (entry) {
		if (entry->crc == crc && entry->len == len &&
		    memcmp(entry->data, data, len) == 0)
			return entry->data;
		entry = entry->next;
	}
	return NULL;
}

void hash_set_add(struct hash_set *hs, void *data, size_t len)
{
	uint32_t crc = rte_hash_crc(data, len, 0);
	struct hash_set_entry *new = hash_set_alloc_entry(hs);

	new->data = data;
	new->len = len;
	new->crc = crc;

	if (hs->mem[crc % hs->n_buckets]) {
		struct hash_set_entry *entry = hs->mem[crc % hs->n_buckets];
		while (entry->next)
			entry = entry->next;
		entry->next = new;
	}
	else {
		hs->mem[crc % hs->n_buckets] = new;
	}
}
