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

#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <inttypes.h>

struct input;

/* command functions */
void start_core_all(int task_id);
void stop_core_all(int task_id);
void start_cores(uint32_t *cores, int count, int task_id);
void stop_cores(uint32_t *cores, int count, int task_id);

void cmd_trace(uint8_t lcore_id, uint8_t task_id, uint32_t nb_packets);
void cmd_dump(uint8_t lcore_id, uint8_t task_id, uint32_t nb_packets, struct input *input, int rx, int tx);
void cmd_mem_stats(void);
void cmd_mem_layout(void);
void cmd_hashdump(uint8_t lcore_id, uint8_t task_id, uint32_t table_id);
void cmd_rx_distr_start(uint32_t lcore_id);
void cmd_rx_distr_stop(uint32_t lcore_id);
void cmd_rx_distr_rst(uint32_t lcore_id);
void cmd_rx_distr_show(uint32_t lcore_id);
void cmd_tx_distr_start(uint32_t lcore_id);
void cmd_tx_distr_stop(uint32_t lcore_id);
void cmd_tx_distr_rst(uint32_t lcore_id);
void cmd_tx_distr_show(uint32_t lcore_id);
void cmd_rx_bw_start(uint32_t lcore_id);
void cmd_tx_bw_start(uint32_t lcore_id);
void cmd_rx_bw_stop(uint32_t lcore_id);
void cmd_tx_bw_stop(uint32_t lcore_id);

void cmd_portinfo(int port_id, char *dst, size_t max_len);
void cmd_port_up(uint8_t port_id);
void cmd_port_down(uint8_t port_id);
void cmd_xstats(uint8_t port_id);
void cmd_thread_info(uint8_t lcore_id, uint8_t task_id);
void cmd_ringinfo(uint8_t lcore_id, uint8_t task_id);
void cmd_ringinfo_all(void);
void cmd_rx_tx_info(void);
void cmd_read_reg(uint8_t port_id, uint32_t id);
void cmd_write_reg(uint8_t port_id, unsigned int id, unsigned int val);
void cmd_set_vlan_filter(uint8_t port_id, unsigned int id, unsigned int val);
void cmd_set_vlan_offload(uint8_t port_id, unsigned int val);

void cmd_reset_port(uint8_t port_id);


#endif /* _COMMANDS_H_ */
