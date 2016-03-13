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

#include "ip_subnet.h"
#include "prox_assert.h"

uint32_t ip4_subet_get_n_hosts(const struct ip4_subnet *sn)
{
	PROX_ASSERT(sn->prefix <= 32 && sn->prefix >= 1);
	return 1 << (32 - sn->prefix);
}

int ip4_subnet_to_host(const struct ip4_subnet *sn, uint32_t host_index, uint32_t *ret_ip)
{
	PROX_ASSERT(ip4_subnet_is_valid(sn));

	if (host_index >= ip4_subet_get_n_hosts(sn)) {
		return -1;
	}

	*ret_ip = sn->ip + host_index;
	return 0;
}

int ip4_subnet_is_valid(const struct ip4_subnet *sn)
{
	if (sn->prefix == 0) {
		return sn->ip == 0;
	}

	return (sn->ip & ~(((int)(1 << 31)) >> (sn->prefix - 1))) == 0;
}
