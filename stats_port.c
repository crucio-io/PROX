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

#include <string.h>
#include <stdio.h>

#include <rte_version.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_byteorder.h>

#include "prox_malloc.h"
#include "log.h"
#include "quit.h"
#include "stats_port.h"
#include "prox_port_cfg.h"
#include "rw_reg.h"

#if defined(PROX_STATS) && defined(PROX_HW_DIRECT_STATS)

/* Directly access hardware counters instead of going through DPDK. This allows getting
 * specific counters that DPDK does not report or aggregates with other ones.
 */

/* Definitions for IXGBE (taken from PMD) */
#define PROX_IXGBE_MPC(_i)           (0x03FA0 + ((_i) * 4)) /* 8 of these 3FA0-3FBC*/
#define PROX_IXGBE_QBRC_L(_i)        (0x01034 + ((_i) * 0x40)) /* 16 of these */
#define PROX_IXGBE_QBRC_H(_i)        (0x01038 + ((_i) * 0x40)) /* 16 of these */
#define PROX_IXGBE_QPRC(_i)          (0x01030 + ((_i) * 0x40)) /* 16 of these */
#define PROX_IXGBE_GPTC              0x04080
#define PROX_IXGBE_TPR               0x040D0
#define PROX_IXGBE_TORL              0x040C0
#define PROX_IXGBE_TORH              0x040C4
#define PROX_IXGBE_GOTCL             0x04090
#define PROX_IXGBE_GOTCH             0x04094

#define IXGBE_QUEUE_STAT_COUNTERS 16

static void ixgbe_read_stats(uint8_t port_id, struct port_stats_sample* stats, struct port_stats_sample *prev, int last_stat)
{
	uint64_t before, after;
	unsigned i;

	struct rte_eth_dev* dev = &rte_eth_devices[port_id];

	/* WARNING: Assumes hardware address is first field of structure! This may change! */
	struct _dev_hw* hw = (struct _dev_hw *)(dev->data->dev_private);

	stats->no_mbufs = dev->data->rx_mbuf_alloc_failed;

	/* Since we only read deltas from the NIC, we have to add to previous values
	 * even though we actually substract again later to find out the rates!
	 */
	stats->ierrors = prev->ierrors;
	stats->rx_bytes = prev->rx_bytes;
	stats->rx_tot = prev->rx_tot;
	stats->tx_bytes = prev->tx_bytes;
	stats->tx_tot = prev->tx_tot;

	/* WARNING: In this implementation, we count as ierrors only the "no descriptor"
	 * missed packets cases and not the actual receive errors.
	 */
	before = rte_rdtsc();
	for (i = 0; i < 8; i++) {
		stats->ierrors += PROX_READ_REG(hw, PROX_IXGBE_MPC(i));
	}

	/* RX stats */
#if 0
	/* This version is equivalent to what ixgbe PMD does. It only accounts for packets
	 * actually received on the host.
	 */
	for (i = 0; i < IXGBE_QUEUE_STAT_COUNTERS; i++) {
		/* ipackets: */
		stats->rx_tot += PROX_READ_REG(hw, PROX_IXGBE_QPRC(i));
		/* ibytes: */
		stats->rx_bytes += PROX_READ_REG(hw, PROX_IXGBE_QBRC_L(i));
		stats->rx_bytes += ((uint64_t)PROX_READ_REG(hw, PROX_IXGBE_QBRC_H(i)) << 32);
	}
#else
	/* This version reports the packets received by the NIC, regardless of whether they
	 * reached the host or not, etc. (no need to add ierrors to this packet count)
	 */
	stats->rx_tot += PROX_READ_REG(hw, PROX_IXGBE_TPR);
	stats->rx_bytes += PROX_READ_REG(hw, PROX_IXGBE_TORL);
	stats->rx_bytes += ((uint64_t)PROX_READ_REG(hw, PROX_IXGBE_TORH) << 32);
#endif

	/* TX stats */
	/* opackets: */
	stats->tx_tot += PROX_READ_REG(hw, PROX_IXGBE_GPTC);
	/* obytes: */
	stats->tx_bytes += PROX_READ_REG(hw, PROX_IXGBE_GOTCL);
	stats->tx_bytes += ((uint64_t)PROX_READ_REG(hw, PROX_IXGBE_GOTCH) << 32);
	after = rte_rdtsc();
	stats->tsc = (before >> 1) + (after >> 1);
}

#endif

extern int last_stat;
static struct port_stats   port_stats[PROX_MAX_PORTS];
static uint8_t nb_interface;
static uint8_t n_ports;
static int num_ixgbe_xstats = 0;

#ifdef RTE_VER_YEAR
#define XSTATS_SUPPORT 1
#else
#if RTE_VERSION >= RTE_VERSION_NUM(2,1,0,0) && RTE_VER_PATCH_RELEASE >= 1
#define XSTATS_SUPPORT 1
#else
#define XSTATS_SUPPORT 0
#endif
#endif

#ifdef XSTATS_SUPPORT
static struct rte_eth_xstats *eth_xstats = NULL;
static int xstat_tpr_offset = -1, xstat_tor_offset = -1;
static int tx_pkt_size_offset[PKT_SIZE_COUNT] = {-1,-1,-1,-1,-1, -1};
#endif
static int find_xstats_str(struct rte_eth_xstats *xstats, int n, const char *name)
{
	for (int i = 0; i < n; i++) {
		if (strcmp(xstats[i].name, name) == 0)
			return i;
	}

	return -1;
}

void stats_port_init(void)
{
#ifdef XSTATS_SUPPORT
	nb_interface = prox_last_port_active() + 1;
	n_ports = prox_nb_active_ports();

	for (uint8_t port_id = 0; port_id < nb_interface; ++port_id) {
		if (!strcmp(prox_port_cfg[port_id].driver_name, "rte_ixgbe_pmd") && prox_port_cfg[port_id].active) {
			num_ixgbe_xstats = rte_eth_xstats_get(port_id, NULL, 0);
			eth_xstats = prox_zmalloc(num_ixgbe_xstats * sizeof(struct rte_eth_xstats), prox_port_cfg[port_id].socket);
			PROX_PANIC(eth_xstats == NULL, "Error allocating memory for xstats");
			num_ixgbe_xstats = rte_eth_xstats_get(port_id, eth_xstats, num_ixgbe_xstats);

			xstat_tor_offset = find_xstats_str(eth_xstats, num_ixgbe_xstats, "rx_total_bytes");
			xstat_tpr_offset = find_xstats_str(eth_xstats, num_ixgbe_xstats, "rx_total_packets");

			tx_pkt_size_offset[PKT_SIZE_64] = find_xstats_str(eth_xstats, num_ixgbe_xstats, "tx_size_64_packets");
			tx_pkt_size_offset[PKT_SIZE_65] = find_xstats_str(eth_xstats, num_ixgbe_xstats, "tx_size_65_to_127_packets");
			tx_pkt_size_offset[PKT_SIZE_128] = find_xstats_str(eth_xstats, num_ixgbe_xstats, "tx_size_128_to_255_packets");
			tx_pkt_size_offset[PKT_SIZE_256] = find_xstats_str(eth_xstats, num_ixgbe_xstats, "tx_size_256_to_511_packets");
			tx_pkt_size_offset[PKT_SIZE_512] = find_xstats_str(eth_xstats, num_ixgbe_xstats, "tx_size_512_to_1023_packets");
			tx_pkt_size_offset[PKT_SIZE_1024] = find_xstats_str(eth_xstats, num_ixgbe_xstats, "tx_size_1024_to_max_packets");

			break;
		}
	}
	if (xstat_tor_offset == -1 ||
	    xstat_tpr_offset == -1 ||
	    num_ixgbe_xstats == 0 ||
	    eth_xstats == NULL) {
		plog_warn("Failed to initialize xstat, running without xstats\n");
		num_ixgbe_xstats = 0;
	}
#endif
}

static void nic_read_stats(uint8_t port_id)
{
	unsigned is_ixgbe = (0 == strcmp(prox_port_cfg[port_id].driver_name, "rte_ixgbe_pmd"));

	struct port_stats_sample *stats = &port_stats[port_id].sample[last_stat];

#if defined(PROX_STATS) && defined(PROX_HW_DIRECT_STATS)
	if (is_ixgbe) {
		struct port_stats_sample *prev = &port_stats[port_id].sample[!last_stat];
		ixgbe_read_stats(port_id, stats, prev, last_stat);
		return;
	}
#endif
	uint64_t before, after;

	struct rte_eth_stats eth_stat;

	before = rte_rdtsc();
	rte_eth_stats_get(port_id, &eth_stat);
	after = rte_rdtsc();

	stats->tsc = (before >> 1) + (after >> 1);
	stats->no_mbufs = eth_stat.rx_nombuf;
	stats->ierrors = eth_stat.ierrors;
	stats->oerrors = eth_stat.oerrors;
	stats->rx_bytes = eth_stat.ibytes;

	/* The goal would be to get the total number of bytes received
	   by the NIC (including overhead). Without the patch
	   (i.e. num_ixgbe_xstats == 0) we can't do this directly with
	   DPDK 2.1 API. So, we report the number of bytes (including
	   overhead) received by the host. */

	if (is_ixgbe) {
#ifdef XSTATS_SUPPORT
		if (num_ixgbe_xstats) {
			rte_eth_xstats_get(port_id, eth_xstats, num_ixgbe_xstats);

			stats->rx_tot = eth_xstats[xstat_tpr_offset].value;
			stats->rx_bytes = eth_xstats[xstat_tor_offset].value;

			for (size_t i = 0; i < sizeof(tx_pkt_size_offset)/sizeof(tx_pkt_size_offset[0]); ++i)
				stats->tx_pkt_size[i] = eth_xstats[tx_pkt_size_offset[i]].value;
		} else
#endif
		{
			stats->rx_tot = eth_stat.ipackets + eth_stat.ierrors;
			/* On ixgbe, the rx_bytes counts bytes
			   received by Host without overhead. The
			   rx_tot counts the number of packets
			   received by the NIC. If we only add 20 *
			   rx_tot to rx_bytes, the result will also
			   take into account 20 * "number of packets
			   dropped by the nic". Note that in case CRC
			   is stripped on ixgbe, the CRC bytes are not
			   counted. */
			if (prox_port_cfg[port_id].port_conf.rxmode.hw_strip_crc == 1)
				stats->rx_bytes = eth_stat.ibytes +
					(24 * eth_stat.ipackets - 20 * eth_stat.ierrors);
			else
				stats->rx_bytes = eth_stat.ibytes +
					(20 * eth_stat.ipackets - 20 * eth_stat.ierrors);
		}
	} else {
		stats->rx_tot = eth_stat.ipackets;
	}
	stats->tx_tot = eth_stat.opackets;
	stats->tx_bytes = eth_stat.obytes;
}

void stats_port_reset(void)
{
	for (uint8_t port_id = 0; port_id < nb_interface; ++port_id) {
		if (prox_port_cfg[port_id].active) {
			rte_eth_stats_reset(port_id);
			memset(&port_stats[port_id], 0, sizeof(struct port_stats));
		}
	}
}

void stats_port_update(void)
{
	for (uint8_t port_id = 0; port_id < nb_interface; ++port_id) {
		if (prox_port_cfg[port_id].active) {
			nic_read_stats(port_id);
		}
	}
}

uint64_t stats_port_get_ierrors(void)
{
	uint64_t ret = 0;

	for (uint8_t port_id = 0; port_id < nb_interface; ++port_id) {
		if (prox_port_cfg[port_id].active)
			ret += port_stats[port_id].sample[last_stat].ierrors;
	}
	return ret;
}

uint64_t stats_port_get_rx_packets(void)
{
	uint64_t ret = 0;

	for (uint8_t port_id = 0; port_id < nb_interface; ++port_id) {
		if (prox_port_cfg[port_id].active)
			ret += port_stats[port_id].sample[last_stat].rx_tot;
	}
	return ret;
}

uint64_t stats_port_get_tx_packets(void)
{
	uint64_t ret = 0;

	for (uint8_t port_id = 0; port_id < nb_interface; ++port_id) {
		if (prox_port_cfg[port_id].active)
			ret += port_stats[port_id].sample[last_stat].tx_tot;
	}
	return ret;
}

int stats_get_n_ports(void)
{
	return n_ports;
}

struct port_stats_sample *stats_get_port_stats_sample(uint32_t port_id, int l)
{
	return &port_stats[port_id].sample[l == last_stat];
}

int stats_port(uint8_t port_id, struct get_port_stats *gps)
{
	if (!prox_port_cfg[port_id].active)
		return -1;

	struct port_stats_sample *last = &port_stats[port_id].sample[last_stat];
	struct port_stats_sample *prev = &port_stats[port_id].sample[!last_stat];

	gps->no_mbufs_diff = last->no_mbufs - prev->no_mbufs;
	gps->ierrors_diff = last->ierrors - prev->ierrors;
	gps->rx_bytes_diff = last->rx_bytes - prev->rx_bytes;
	gps->tx_bytes_diff = last->tx_bytes - prev->tx_bytes;
	gps->rx_pkts_diff = last->rx_tot - prev->rx_tot;
	gps->tx_pkts_diff = last->tx_tot - prev->tx_tot;

	gps->rx_tot = last->rx_tot;
	gps->tx_tot = last->tx_tot;
	gps->no_mbufs_tot = last->no_mbufs;
	gps->ierrors_tot = last->ierrors;

	gps->last_tsc = last->tsc;
	gps->prev_tsc = prev->tsc;

	return 0;
}
