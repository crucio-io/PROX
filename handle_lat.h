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

#ifndef _HANDLE_LAT_H_
#define _HANDLE_LAT_H_

#include <stdio.h>
#include "task_base.h"

#define MAX_PACKETS_FOR_LATENCY 64
#define LATENCY_ACCURACY	1
#define DEFAULT_BUCKET_SIZE	10

#define MAX_NB_QUEUES          16

#define PACKET_QUEUE_BITS      14
#define PACKET_QUEUE_SIZE      (1 << PACKET_QUEUE_BITS)
#define PACKET_QUEUE_MASK      (PACKET_QUEUE_SIZE - 1)

#define QUEUE_ID_BITS		(32 - PACKET_QUEUE_BITS)
#define QUEUE_ID_SIZE		(1 << QUEUE_ID_BITS)
#define QUEUE_ID_MASK		(QUEUE_ID_SIZE - 1)

struct lat_test {
	uint64_t max_lat;
	uint64_t min_lat;
	uint64_t tot_lat;
	uint64_t var_lat; /* variance*/
	uint64_t min_rx_acc;
	uint64_t max_rx_acc;
	uint64_t min_tx_acc;
	uint64_t max_tx_acc;
	uint64_t tot_rx_acc;
	uint64_t tot_tx_acc;
	uint64_t tot_pkts;
	uint64_t buckets[128];
	uint64_t lost_packets;
#ifdef LATENCY_PER_PACKET
	uint32_t cur_pkt;
	uint64_t lat[MAX_PACKETS_FOR_LATENCY];
#endif
};

struct lat_info {
	uint32_t rx_packet_index;
	uint32_t tx_packet_index;
	uint32_t lat;
	uint32_t tx_err;
	uint32_t rx_err;
	uint64_t rx_time;
	uint64_t tx_time;
	uint16_t port_queue_id;
#ifdef LAT_DEBUG
	uint16_t packet_id;
	uint16_t bulk_size;
	uint64_t begin;
	uint64_t after;
	uint64_t before;
#endif
};

struct task_lat {
	struct task_base base;
	uint64_t lost_packets;
	uint64_t rx_packet_index;
	uint64_t last_pkts_tsc;
	struct lat_info *latency_buffer;
	uint32_t latency_buffer_size;
	uint64_t begin;
	uint32_t bucket_size;
	uint16_t lat_pos;
	uint16_t packet_id_pos;
	uint16_t accur_pos;
	volatile uint16_t use_lt; /* which lt to use, */
	volatile uint16_t using_lt; /* 0 or 1 depending on which of the 2 result records are used */
	struct lat_test lt[2];
	uint32_t queue[MAX_NB_QUEUES][PACKET_QUEUE_SIZE];
	uint32_t tx_packet_index[MAX_NB_QUEUES];
	uint32_t *pkt_tx_time;
	uint8_t **hdr;
	FILE *fp_rx;
	FILE *fp_tx;
};

#endif /* _HANDLE_LAT_H_ */
