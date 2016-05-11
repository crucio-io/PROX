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

#include <rte_hash_crc.h>
#include <stdint.h>

#include "prox_malloc.h"

#define KV_STORE_BUCKET_DEPTH 8

struct kv_store_expire_entry {
	/* if set to 0, the entry is disabled */
	uint64_t timeout;
	/* Memory contains the key, followed by the actual value. */
	uint8_t  mem[0];
};

struct kv_store_expire {
	size_t key_size;
	size_t entry_size;
	size_t bucket_mask;
	size_t bucket_size;
	uint64_t timeout;

	void (*expire)(void *entry_value);

	uint8_t mem[0];
};

static struct kv_store_expire *kv_store_expire_create(uint32_t n_entries, size_t key_size, size_t value_size, int socket, void (*expire)(void *entry_value), uint64_t timeout)
{
	struct kv_store_expire *ret;
	size_t memsize = 0;
	size_t bucket_size;
	size_t entry_size;

	if (!rte_is_power_of_2(n_entries))
		n_entries = rte_align32pow2(n_entries);
	entry_size = sizeof(struct kv_store_expire_entry) + key_size + value_size;

	memsize += sizeof(struct kv_store_expire);
	memsize += entry_size * n_entries;

	ret = prox_zmalloc(memsize, socket);
	if (ret == NULL)
		return NULL;

	ret->bucket_mask = n_entries / KV_STORE_BUCKET_DEPTH - 1;
	ret->bucket_size = entry_size * KV_STORE_BUCKET_DEPTH;
	ret->entry_size = entry_size;
	ret->key_size = key_size;
	ret->expire = expire;
	ret->timeout = timeout;

	return ret;
}

static size_t kv_store_expire_size(struct kv_store_expire *kv_store)
{
	return (kv_store->bucket_mask + 1) * KV_STORE_BUCKET_DEPTH;
}

static void entry_set_timeout(struct kv_store_expire_entry *entry, uint64_t timeout)
{
	entry->timeout = timeout;
}

static struct kv_store_expire_entry *entry_next(struct kv_store_expire *kv_store, struct kv_store_expire_entry *entry)
{
	return (struct kv_store_expire_entry *)((uint8_t *)entry + kv_store->entry_size);
}

static void *entry_key(__attribute__((unused)) struct kv_store_expire *kv_store, struct kv_store_expire_entry *entry)
{
	return (uint8_t *)entry->mem;
}

static void *entry_value(struct kv_store_expire *kv_store, struct kv_store_expire_entry *entry)
{
	return (uint8_t *)entry->mem + kv_store->key_size;
}

static struct kv_store_expire_entry *kv_store_expire_get_first(struct kv_store_expire *kv_store)
{
	return (struct kv_store_expire_entry *)&kv_store->mem[0];
}

static struct kv_store_expire_entry *kv_store_expire_get_first_in_bucket(struct kv_store_expire *kv_store, void *key)
{
	uint32_t key_hash = rte_hash_crc(key, kv_store->key_size, 0);
	uint32_t bucket_idx = key_hash & kv_store->bucket_mask;

	return (struct kv_store_expire_entry *)&kv_store->mem[bucket_idx * kv_store->bucket_size];
}

static int entry_key_matches(struct kv_store_expire *kv_store, struct kv_store_expire_entry *entry, void *key)
{
	return !memcmp(entry_key(kv_store, entry), key, kv_store->key_size);
}

static struct kv_store_expire_entry *kv_store_expire_get(struct kv_store_expire *kv_store, void *key, uint64_t now)
{
	struct kv_store_expire_entry *entry = kv_store_expire_get_first_in_bucket(kv_store, key);

	for (int i = 0; i < KV_STORE_BUCKET_DEPTH; ++i) {
		if (entry->timeout && entry->timeout >= now) {
			if (entry_key_matches(kv_store, entry, key)) {
				entry->timeout = now + kv_store->timeout;
				return entry;
			}
		}
		entry = entry_next(kv_store, entry);
	}
	return NULL;
}

static struct kv_store_expire_entry *kv_store_expire_put(struct kv_store_expire *kv_store, void *key, uint64_t now)
{
	struct kv_store_expire_entry *e = kv_store_expire_get_first_in_bucket(kv_store, key);

	for (int i = 0; i < KV_STORE_BUCKET_DEPTH; ++i) {
		if (e->timeout && e->timeout >= now) {
			e = entry_next(kv_store, e);
			continue;
		}
		if (!e->timeout) {
			kv_store->expire(entry_value(kv_store, e));
		}

		rte_memcpy(entry_key(kv_store, e), key, kv_store->key_size);
		e->timeout = now + kv_store->timeout;
		return e;
	}

	return NULL;
}

/* If the entry is not found, a put operation is tried and if that
   succeeds, that entry is returned. The bucket is full if NULL Is
   returned. */
static struct kv_store_expire_entry *kv_store_expire_get_or_put(struct kv_store_expire *kv_store, void *key, uint64_t now)
{
	struct kv_store_expire_entry *entry = kv_store_expire_get_first_in_bucket(kv_store, key);
	struct kv_store_expire_entry *v = NULL;

	for (int i = 0; i < KV_STORE_BUCKET_DEPTH; ++i) {
		if (entry->timeout && entry->timeout >= now) {
			if (entry_key_matches(kv_store, entry, key)) {
				entry->timeout = now + kv_store->timeout;
				return entry;
			}
		}
		else {
			v = v? v : entry;
		}
		entry = entry_next(kv_store, entry);
	}

	if (v) {
		if (entry->timeout)
			kv_store->expire(entry_value(kv_store, v));
		rte_memcpy(entry_key(kv_store, v), key, kv_store->key_size);
		v->timeout = now + kv_store->timeout;
		return v;
	}

	return NULL;
}

static size_t kv_store_expire_expire_all(struct kv_store_expire *kv_store)
{
	struct kv_store_expire_entry *entry = kv_store_expire_get_first(kv_store);
	size_t elems = kv_store_expire_size(kv_store);
	size_t expired = 0;

	do {
		if (entry->timeout) {
			kv_store->expire(entry_value(kv_store, entry));
			entry->timeout = 0;
			expired++;
		}
		entry = entry_next(kv_store, entry);
	} while (--elems);
	return expired;
}
