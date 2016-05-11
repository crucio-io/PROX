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

#include "task_init.h"
#include "task_base.h"
#include "stats.h"
#include "arp.h"
#include "etypes.h"
#include "quit.h"
#include "log.h"
#include "prox_port_cfg.h"
#include "lconf.h"

#define MAX_LOST_PACKETS       (32768 * 16)
#define PACKET_QUEUE_BITS      14
#define PACKET_QUEUE_SIZE      (1 << PACKET_QUEUE_BITS)
#define PACKET_QUEUE_MASK      (PACKET_QUEUE_SIZE - 1)

struct task_drop {
	struct task_base   base;
	struct ether_addr  src_mac;
	uint32_t packet_id_pos;
	uint32_t nb_lost_packets;
	int32_t queue[PACKET_QUEUE_SIZE];
	uint64_t lost_packet[MAX_LOST_PACKETS];
};

static void handle_drop_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	tbase->tx_pkt(tbase, mbufs, n_pkts, NULL);
}

static int compare(void const *val1, void const *val2)
{
	uint64_t const *ptr1 = val1;
	uint64_t const *ptr2 = val2;

	return *ptr1 - *ptr2;
}

static void drop_stop(struct task_base *tbase)
{
	struct task_drop *task = (struct task_drop *)tbase;

	if (task->nb_lost_packets) {
		int64_t first = -1, last = 0;
		plogx_info("Lost %d packet in total\n", task->nb_lost_packets);

		// Note the the following is wrong when lost_packet contains unordered packets
		qsort (task->lost_packet, task->nb_lost_packets, 8, compare);
		for (uint32_t i = 0; i < task->nb_lost_packets; i++) {
			if (first == -1) {
				first = task->lost_packet[i];
				last = first;
		 	} else if (task->lost_packet[i] - last != 1) {
				plogx_info("Lost %ld packet from %ld to %ld\n", last - first + 1, first, last);
				first = task->lost_packet[i];
				last = first;
			} else {
				last = task->lost_packet[i];
			}
		}
		plogx_info("Lost %ld packet from %ld to %ld\n", last - first + 1, first, last);
		task->nb_lost_packets = 0;
	}
}

static inline void prepare_arp_reply(struct task_drop *task, struct ether_hdr_arp *packet)
{
	uint32_t ip_source = packet->arp.data.spa;

	packet->arp.data.spa = packet->arp.data.tpa;
	packet->arp.data.tpa = ip_source;
	memcpy(&packet->arp.data.tha, &packet->arp.data.sha, sizeof(struct ether_addr));
	memcpy(&packet->arp.data.sha, &task->src_mac, sizeof(struct ether_addr));
}

static void handle_drop_arp_reply_bulk(struct task_base *tbase, struct rte_mbuf **mbufs, uint16_t n_pkts)
{
	struct ether_hdr_arp *hdr;
	struct task_drop *task = (struct task_drop *)tbase;
	uint8_t out[MAX_PKT_BURST];
	int32_t old_queue_id, queue_id, queue_pos, nb_loss ;
	uint64_t packet_index;

	for (uint16_t j = 0; j < n_pkts; ++j) {
		hdr = rte_pktmbuf_mtod(mbufs[j], struct ether_hdr_arp *);
		if (hdr->ether_hdr.ether_type == ETYPE_ARP) {
			prepare_arp_reply(task, hdr);
			memcpy(hdr->ether_hdr.d_addr.addr_bytes, hdr->ether_hdr.s_addr.addr_bytes, 6);
			memcpy(hdr->ether_hdr.s_addr.addr_bytes, &task->src_mac, 6);
			out[j] = 0;
			hdr->arp.oper = 0x200;
		} else {
			// If an ID was wriiten in the packet, check whether a packet has been lost
			if (task->packet_id_pos) {
				packet_index = *(uint32_t *)(rte_pktmbuf_mtod(mbufs[j], uint8_t *) + task->packet_id_pos);
				queue_pos = packet_index & PACKET_QUEUE_MASK;
				old_queue_id = task->queue[queue_pos];
				task->queue[queue_pos] = packet_index >> PACKET_QUEUE_BITS;
				nb_loss = task->queue[queue_pos] - old_queue_id - 1;
				if (nb_loss > 0) {
					for (int j = 1; j < nb_loss + 1; j++) {
						task->lost_packet[task->nb_lost_packets++] = packet_index - j * PACKET_QUEUE_SIZE;
						if (task->nb_lost_packets == MAX_LOST_PACKETS) {
							plogx_info("Lost %d packets\n", task->nb_lost_packets);
							task->nb_lost_packets = 0;
						}
					}
				}
			}
			out[j] = -1;
		}
	}
	task->base.tx_pkt(&task->base, mbufs, n_pkts, out);
}

static void init_task_drop_arp_reply(struct task_base *tbase, struct task_args *targ)
{
	struct task_drop *task = (struct task_drop *)tbase;

	PROX_PANIC(targ->nb_txports == 0, "drop mode with arp_reply must have a tx_port");
	memcpy(&task->src_mac, &prox_port_cfg[task->base.tx_params_hw.tx_port_queue[0].port].eth_addr, sizeof(struct ether_addr));
	task->packet_id_pos = targ->packet_id_pos;
	if (task->packet_id_pos) {
		for (int i = 0; i < PACKET_QUEUE_SIZE; i++) {
			task->queue[i] = -1;
		}
	}
	plog_info("Running drop mode with arp_reply with id_pos = %d\n",  task->packet_id_pos);
}

static struct task_init task_init_drop = {
	.mode_str = "drop",
	.init = NULL,
	.flag_features = TASK_FEATURE_NEVER_DISCARDS,
	.handle = handle_drop_bulk,
	.size = sizeof(struct task_base)
};

static struct task_init task_init_drop_arp_reply = {
	.mode_str = "drop",
	.sub_mode_str = "with_arp_reply",
	.init = init_task_drop_arp_reply,
	.handle = handle_drop_arp_reply_bulk,
	.stop = drop_stop,
	.flag_features = 0,
	.size = sizeof(struct task_drop)
};

__attribute__((constructor)) static void reg_task_drop(void)
{
	reg_task(&task_init_drop);
	reg_task(&task_init_drop_arp_reply);
}
