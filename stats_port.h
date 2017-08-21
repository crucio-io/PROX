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

#ifndef _STATS_PORT_H_
#define _STATS_PORT_H_

#include <inttypes.h>

enum PKT_SIZE_BIN {
	PKT_SIZE_64,
	PKT_SIZE_65,
	PKT_SIZE_128,
	PKT_SIZE_256,
	PKT_SIZE_512,
	PKT_SIZE_1024,
	PKT_SIZE_1522,
	PKT_SIZE_COUNT,
};

struct port_stats_sample {
	uint64_t tsc;
	uint64_t no_mbufs;
	uint64_t ierrors;
	uint64_t imissed;
	uint64_t oerrors;
	uint64_t rx_tot;
	uint64_t tx_tot;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint64_t tx_pkt_size[PKT_SIZE_COUNT];
};

struct port_stats {
	struct port_stats_sample sample[2];
};

struct get_port_stats {
	uint64_t no_mbufs_diff;
	uint64_t ierrors_diff;
	uint64_t imissed_diff;
	uint64_t rx_bytes_diff;
	uint64_t tx_bytes_diff;
	uint64_t rx_pkts_diff;
	uint64_t tx_pkts_diff;
	uint64_t rx_tot;
	uint64_t tx_tot;
	uint64_t no_mbufs_tot;
	uint64_t ierrors_tot;
	uint64_t imissed_tot;
	uint64_t last_tsc;
	uint64_t prev_tsc;
};

int stats_port(uint8_t port_id, struct get_port_stats *ps);
void stats_port_init(void);
void stats_port_reset(void);
void stats_port_update(void);
uint64_t stats_port_get_ierrors(void);
uint64_t stats_port_get_imissed(void);
uint64_t stats_port_get_rx_packets(void);
uint64_t stats_port_get_tx_packets(void);

int stats_get_n_ports(void);
struct port_stats_sample *stats_get_port_stats_sample(uint32_t port_id, int l);

#endif /* _STATS_PORT_H_ */
