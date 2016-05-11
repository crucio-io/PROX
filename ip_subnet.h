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

#ifndef _IP_SUBNET_H_
#define _IP_SUBNET_H_

#include <inttypes.h>

struct ip4_subnet {
	uint32_t ip;
	uint8_t prefix; /* always in range [1,32] inclusive */
};

struct ip6_subnet {
	uint8_t ip[16];
	uint8_t prefix; /* always in range [1,128] inclusive */
};

/* Returns number of hosts (assuming that network address and
   broadcast address are both hosts) within the subnet. */
uint32_t ip4_subet_get_n_hosts(const struct ip4_subnet *sn);

/* Allows to get a specific host within a subnet. Note that the
   network address and broadcast address are both considered to
   "hosts". Setting host_index to 0 returns the network address and
   setting the host_index to the last host within the subnet returns
   the broadcast. To get all addresses with the subnet, loop
   host_index from 0 to ip_subnet_get_n_hosts(). */
int ip4_subnet_to_host(const struct ip4_subnet* sn, uint32_t host_index, uint32_t* ret_ip);

/* Check if IP address is a network address (i.e. all bits outside the
   prefix are set to 0). */
int ip4_subnet_is_valid(const struct ip4_subnet* sn);

#endif /* _IP_SUBNET_H_ */
