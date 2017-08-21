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

#ifndef _PROX_LUA_TYPES_H_
#define _PROX_LUA_TYPES_H_

#include <inttypes.h>
#include <rte_ether.h>
#include <rte_hash.h>

#include "ip6_addr.h"

struct lua_State;
struct ether_addr;
struct ip4_subnet;
struct ip6_subnet;
struct next_hop;
struct rte_lpm;
struct rte_lpm6;
struct next_hop6;
struct rte_acl_ctx;
struct qinq_gre_map;

#define MAX_HOP_INDEX  128
enum l4gen_peer {PEER_SERVER, PEER_CLIENT};

static const char *l4gen_peer_to_str(enum l4gen_peer peer)
{
	return peer == PEER_SERVER? "server" : "client";
}

struct peer_data {
	uint8_t *hdr;
	uint32_t hdr_len;
	uint8_t *content;
};

struct peer_action {
	enum l4gen_peer   peer;
	uint32_t          beg;
	uint32_t          len;
};

struct lpm4 {
	uint32_t n_free_rules;
	uint32_t n_used_rules;
	struct next_hop *next_hops;
	struct rte_lpm *rte_lpm;
};

struct lpm6 {
	struct rte_lpm6 *rte_lpm6;
	struct next_hop6 *next_hops;
	uint32_t n_free_rules;
	uint32_t n_used_rules;
};

struct ipv6_tun_binding_entry {
	struct ipv6_addr        endpoint_addr;  // IPv6 local addr
	struct ether_addr       next_hop_mac;   // mac addr of next hop towards lwB4
	uint32_t                public_ipv4;    // Public IPv4 address
	uint16_t                public_port;    // Public base port (together with port mask, defines the Port Set)
} __attribute__((__packed__));

struct ipv6_tun_binding_table {
	uint32_t                num_binding_entries;
	struct ipv6_tun_binding_entry entry[0];
};

struct cpe_table_entry {
	uint32_t port_idx;
	uint32_t gre_id;
	uint32_t svlan;
	uint32_t cvlan;
	uint32_t ip;
	struct ether_addr eth_addr;
	uint32_t user;
};

struct cpe_table_data {
	uint32_t               n_entries;
	struct cpe_table_entry entries[0];
};

struct val_mask {
	uint32_t val;
	uint32_t mask;
};

struct val_range {
	uint32_t beg;
	uint32_t end;
};

enum acl_action {ACL_NOT_SET, ACL_ALLOW, ACL_DROP, ACL_RATE_LIMIT};

const char *get_lua_to_errors(void);

enum lua_place {STACK, TABLE, GLOBAL};
int lua_getfrom(struct lua_State *L, enum lua_place from, const char *name);

int lua_to_port(struct lua_State *L, enum lua_place from, const char *name, uint16_t *port);
int lua_to_ip(struct lua_State *L, enum lua_place from, const char *name, uint32_t *ip);
int lua_to_ip6(struct lua_State *L, enum lua_place from, const char *name, uint8_t *ip);
int lua_to_mac(struct lua_State *L, enum lua_place from, const char *name, struct ether_addr *mac);
int lua_to_cidr(struct lua_State *L, enum lua_place from, const char *name, struct ip4_subnet *cidr);
int lua_to_cidr6(struct lua_State *L, enum lua_place from, const char *name, struct ip6_subnet *cidr);
int lua_to_int(struct lua_State *L, enum lua_place from, const char *name, uint32_t *val);
int lua_to_double(struct lua_State *L, enum lua_place from, const char *name, double *val);
int lua_to_string(struct lua_State *L, enum lua_place from, const char *name, char *dst, size_t size);
int lua_to_val_mask(struct lua_State *L, enum lua_place from, const char *name, struct val_mask *val_mask);
int lua_to_val_range(struct lua_State *L, enum lua_place from, const char *name, struct val_range *val_range);
int lua_to_action(struct lua_State *L, enum lua_place from, const char *name, enum acl_action *action);
int lua_to_dscp(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, uint8_t **dscp);
int lua_to_user_table(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, uint16_t **user_table);
int lua_to_lpm4(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct lpm4 **lpm);
int lua_to_routes4(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct lpm4 *lpm);
int lua_to_next_hop(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct next_hop **nh);
int lua_to_lpm6(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct lpm6 **lpm);
int lua_to_ip6_tun_binding(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct ipv6_tun_binding_table **data);
int lua_to_qinq_gre_map(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct qinq_gre_map **qinq_gre_map);
int lua_to_cpe_table_data(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct cpe_table_data **data);
int lua_to_rules(struct lua_State *L, enum lua_place from, const char *name, struct rte_acl_ctx *ctx, uint32_t* n_max_rules, int use_qinq, uint16_t qinq_tag);
int lua_to_routes4_entry(struct lua_State *L, enum lua_place from, const char *name, struct ip4_subnet *cidr, uint32_t *nh_idx);
int lua_to_next_hop6(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct next_hop6 **nh);
int lua_to_routes6(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct lpm6 *lpm);
int lua_to_tuples(struct lua_State *L, enum lua_place from, const char *name, uint8_t socket, struct rte_hash **lookup_hash, uint8_t **out_if);

#endif /* _PROX_LUA_TYPES_H_ */
