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

#include "genl4_stream_udp.h"
#include "mbuf_utils.h"

int stream_udp_is_ended(struct stream_ctx *ctx)
{
	return ctx->cur_action == ctx->stream_cfg->n_actions;
}

int stream_udp_proc(struct stream_ctx *ctx, struct rte_mbuf *mbuf, struct l4_meta *l4_meta, uint64_t *next_tsc, __attribute__((unused)) uint32_t *retransmit)
{
	if (l4_meta) {
		enum l4gen_peer peer = ctx->stream_cfg->actions[ctx->cur_action].peer;
		plogx_dbg("Consuming UDP data\n");
		/* data should come from the other side */
		if (peer == ctx->peer) {
			plogx_err("Wrong peer\n");
			return -1;
		}
		/* Fixed length data expected */
		if (ctx->stream_cfg->actions[ctx->cur_action].len != l4_meta->len) {
			plogx_dbg("unexpected UDP len (expected = %u, got = %u, action = %u)\n",
				  ctx->stream_cfg->actions[ctx->cur_action].len,
				  l4_meta->len,
				  ctx->cur_action);

			return -1;
		}
		/* With specific payload */
		if (memcmp(ctx->stream_cfg->data[peer].content + ctx->cur_pos[peer], l4_meta->payload, l4_meta->len) != 0) {
			plogx_dbg("Bad payload at action_id %d\n", ctx->cur_action);
			return -1;
		}
		ctx->cur_pos[peer] += l4_meta->len;
		ctx->cur_action++;

		if (stream_udp_is_ended(ctx))
			return -1;

		token_time_update(&ctx->token_time_other, rte_rdtsc());
		token_time_take(&ctx->token_time_other, mbuf_wire_size(mbuf));
		/* Time before next packet is expected to
		   arrive. Note, addition amount of time is accounted
		   for due to rate limiting. */
		uint64_t wait = token_time_tsc_until_full(&ctx->token_time_other);
		*next_tsc = wait + ctx->stream_cfg->tsc_timeout;
	}

	if (ctx->stream_cfg->actions[ctx->cur_action].peer != ctx->peer) {
		const char *other_peer_str = ctx->peer != PEER_SERVER? "server" : "client";

		plogx_dbg("Expecting more UDP data from %s, will expire = %s\n", other_peer_str, l4_meta == NULL? "yes" : "no");
		if (!l4_meta) {
			ctx->flags |= STREAM_CTX_F_EXPIRED;
		}
		return -1;
	}

	token_time_update(&ctx->token_time, rte_rdtsc());
	uint64_t wait_tsc = token_time_tsc_until_full(&ctx->token_time);

	if (wait_tsc != 0) {
		plogx_dbg("Wait = %"PRIu64"\n", wait_tsc);
		*next_tsc = wait_tsc;
		return -1;
	}

	const struct stream_cfg *stream_cfg = ctx->stream_cfg;

	uint8_t *pkt = rte_pktmbuf_mtod(mbuf, uint8_t *);
	const struct peer_action *act = &stream_cfg->actions[ctx->cur_action];

	uint16_t pkt_len = stream_cfg->data[act->peer].hdr_len + sizeof(struct udp_hdr) + act->len;

	rte_pktmbuf_pkt_len(mbuf) = pkt_len;
	rte_pktmbuf_data_len(mbuf) = pkt_len;
	plogx_dbg("Creating UDP data (peer = %s, payload len = %u)\n", act->peer == PEER_CLIENT? "client" : "server", act->len);
	/* Construct the packet. The template is used up to L4 header,
	   a gap of sizeof(l4_hdr) is skipped, followed by the payload. */
	rte_memcpy(pkt, stream_cfg->data[act->peer].hdr, stream_cfg->data[act->peer].hdr_len);
	rte_memcpy(pkt + stream_cfg->data[act->peer].hdr_len + sizeof(struct udp_hdr), stream_cfg->data[act->peer].content + act->beg, act->len);

	struct ipv4_hdr *l3_hdr = (struct ipv4_hdr*)&pkt[stream_cfg->data[act->peer].hdr_len - sizeof(struct ipv4_hdr)];
	struct udp_hdr *l4_hdr = (struct udp_hdr*)&pkt[stream_cfg->data[act->peer].hdr_len];

	l3_hdr->src_addr = ctx->tuple->dst_addr;
	l3_hdr->dst_addr = ctx->tuple->src_addr;
	l3_hdr->next_proto_id = IPPROTO_UDP;
	l4_hdr->src_port = ctx->tuple->dst_port;
	l4_hdr->dst_port = ctx->tuple->src_port;
	l4_hdr->dgram_len = rte_bswap16(sizeof(struct udp_hdr) + act->len);
	/* TODO: UDP checksum calculation */
	l3_hdr->total_length = rte_bswap16(sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + act->len);
	ctx->cur_pos[ctx->peer] += act->len;
	ctx->cur_action++;

	/* When the stream has ended, there is no need to schedule
	   another timeout (which will be unscheduled at the end of
	   the stream). */
	if (stream_udp_is_ended(ctx))
		return 0;

	token_time_take(&ctx->token_time, mbuf_wire_size(mbuf));

	/* Send next packet as soon as possible */
	if (ctx->stream_cfg->actions[ctx->cur_action].peer == ctx->peer) {
		*next_tsc = token_time_tsc_until_full(&ctx->token_time);
	}
	else {
		token_time_update(&ctx->token_time_other, rte_rdtsc());
		uint64_t wait = token_time_tsc_until_full(&ctx->token_time_other);
		*next_tsc = wait + ctx->stream_cfg->tsc_timeout;
	}

	return 0;
}

uint16_t stream_udp_reply_len(struct stream_ctx *ctx)
{
	if (stream_udp_is_ended(ctx))
		return 0;
	else if (ctx->stream_cfg->actions[ctx->cur_action].peer == ctx->peer)
		return 0;
	else
		return ctx->stream_cfg->data[ctx->stream_cfg->actions[ctx->cur_action].peer].hdr_len + sizeof(struct udp_hdr) +
			ctx->stream_cfg->actions[ctx->cur_action].len;
}

void stream_udp_calc_len(struct stream_cfg *cfg, uint32_t *n_pkts, uint32_t *n_bytes)
{
	const uint32_t client_hdr_len = cfg->data[PEER_CLIENT].hdr_len;
	const uint32_t server_hdr_len = cfg->data[PEER_SERVER].hdr_len;

	*n_pkts = 0;
	*n_bytes = 0;

	for (uint32_t i = 0; i < cfg->n_actions; ++i) {
		const uint32_t send_hdr_len = cfg->actions[i].peer == PEER_CLIENT? client_hdr_len : server_hdr_len;
		uint32_t len = send_hdr_len + sizeof(struct udp_hdr) + cfg->actions[i].len;
		*n_bytes += (len < 60? 60 : len) + 24;
		(*n_pkts)++;
	}
}
