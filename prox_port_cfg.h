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

#ifndef _PROX_PORT_CFG_H
#define _PROX_PORT_CFG_H

#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include "prox_globals.h"
#include "ipv4_range_parser.h"

enum addr_type {PROX_PORT_MAC_HW, PROX_PORT_MAC_SET, PROX_PORT_MAC_RAND};

#define IPV4_CKSUM	1
#define UDP_CKSUM	2

struct prox_port_cfg {
	struct rte_mempool *pool[32];  /* Rx/Tx mempool */
	size_t pool_size[32];
	uint8_t promiscuous;
	uint8_t lsc_set_explicitely; /* Explicitly enable/disable lsc */
	uint8_t lsc_val;
	uint8_t active;
	int socket;
	uint16_t max_rxq;         /* max number of Tx queues */
	uint16_t max_txq;         /* max number of Tx queues */
	uint16_t n_rxq;           /* number of used Rx queues */
	uint16_t n_txq;           /* number of used Tx queues */
	uint32_t n_rxd;
	uint32_t n_txd;
	uint8_t  link_up;
	uint32_t  link_speed;
	uint32_t  mtu;
	enum addr_type    type;
	struct ether_addr eth_addr;    /* port MAC address */
	char name[MAX_NAME_SIZE];
	char short_name[MAX_NAME_SIZE];
	char driver_name[MAX_NAME_SIZE];
	char rx_ring[MAX_NAME_SIZE];
	char tx_ring[MAX_NAME_SIZE];
	char pci_addr[32];
	struct rte_eth_conf port_conf;
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_txconf tx_conf;
	struct {
		int tx_offload_cksum;
	} capabilities;
};

extern rte_atomic32_t lsc;

int prox_nb_active_ports(void);
int prox_last_port_active(void);

extern struct prox_port_cfg prox_port_cfg[];

void init_rte_dev(int use_dummy_devices);
uint8_t init_rte_ring_dev(void);
void init_port_addr(void);
void init_port_all(void);
void close_ports_atexit(void);

struct rte_mempool;

void prox_pktmbuf_init(struct rte_mempool *mp, void *opaque_arg, void *_m, unsigned i);
void prox_pktmbuf_reinit(void *arg, void *start, void *end, uint32_t idx);

int port_is_active(uint8_t port_id);

#endif /* __PROX_PORT_CFG_H_ */
