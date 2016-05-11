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

#ifndef _GENL4_BUNDLE_H_
#define _GENL4_BUNDLE_H_

#include "heap.h"
#include "genl4_stream.h"
#include "lconf.h"

/* Configured once and used during packet generation. The structure
   describes a single set of consecutive streams. When used at the
   server side, it only contains a simple stream to represent a
   service. */
struct bundle_cfg {
	struct host_set   clients;
	uint32_t          n_stream_cfgs;
	struct stream_cfg **stream_cfgs;
};

/* A bundle_ctx represents a an active stream between a client and a
   server of servers. */
struct bundle_ctx {
	struct pkt_tuple        tuple;      /* Client IP/PORT generated once at bundle creation time, client PORT and server IP/PORT created when stream_idx++ */
	struct heap_ref         heap_ref;   /* Back reference into heap */
	struct heap             *heap;      /* timer management */

	const struct bundle_cfg *cfg;       /* configuration time read only structure */

	struct stream_ctx       ctx;        /* state management info for stream_cfg (reset when stream_idx++) */
	uint32_t                stream_idx; /* iterate through cfg->straem_cfgs */
};

#define BUNDLE_CTX_UPCAST(r) ((struct bundle_ctx *)((uint8_t *)r - offsetof(struct bundle_ctx, heap_ref)))

struct bundle_ctx_pool {
	struct rte_hash   *hash;
	struct bundle_ctx **hash_entries;
	struct bundle_ctx **free_bundles;
	struct bundle_ctx *bundles; /* Memory containing all communications */
	uint32_t          *occur;
	struct bundle_cfg *bundle_cfg;
	uint32_t          n_occur;
	uint32_t          seed;
	uint32_t          n_free_bundles;
	uint32_t          tot_bundles;
};

struct l4_stats {
	uint64_t bundles_created;
	uint64_t tcp_finished_no_retransmit;
	uint64_t tcp_finished_retransmit;
	uint64_t udp_finished;
	uint64_t tcp_created;
	uint64_t udp_created;
	uint64_t tcp_expired;
	uint64_t tcp_retransmits;
	uint64_t udp_expired;
};

struct cdf;
int bundle_ctx_pool_create(const char *name, uint32_t n_elems, struct bundle_ctx_pool *ret, uint32_t *occur, uint32_t n_occur, struct bundle_cfg *cfg, int socket_id);

struct bundle_ctx *bundle_ctx_pool_get(struct bundle_ctx_pool *p);
struct bundle_ctx *bundle_ctx_pool_get_w_cfg(struct bundle_ctx_pool *p);
void bundle_ctx_pool_put(struct bundle_ctx_pool *p, struct bundle_ctx *bundle);

void bundle_create_tuple(struct pkt_tuple *tp, const struct host_set *clients, const struct stream_cfg *stream_cfg, int rnd_ip, unsigned *seed);
void bundle_init(struct bundle_ctx *bundle, struct heap *heap, enum l4gen_peer peer, unsigned *seed);
void bundle_init_w_cfg(struct bundle_ctx *bundle, const struct bundle_cfg *cfg, struct heap *heap, enum l4gen_peer peer, unsigned *seed);
void bundle_expire(struct bundle_ctx *bundle, struct bundle_ctx_pool *pool, struct l4_stats *l4_stats);
int bundle_proc_data(struct bundle_ctx *bundle, struct rte_mbuf *mbuf, struct l4_meta *l4_meta, struct bundle_ctx_pool *pool, unsigned *seed, struct l4_stats *l4_stats);
uint32_t bundle_cfg_length(struct bundle_cfg *cfg);
uint32_t bundle_cfg_max_n_segments(struct bundle_cfg *cfg);

#endif /* _GENL4_BUNDLE_H_ */
