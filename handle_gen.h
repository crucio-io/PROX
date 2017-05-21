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

#ifndef _HANDLE_GEN_H_
#define _HANDLE_GEN_H_

struct unique_id {
	uint8_t  generator_id;
	uint32_t packet_id;
} __attribute__((packed));

static void unique_id_init(struct unique_id *unique_id, uint8_t generator_id, uint32_t packet_id)
{
	unique_id->generator_id = generator_id;
	unique_id->packet_id = packet_id;
}

static void unique_id_get(struct unique_id *unique_id, uint8_t *generator_id, uint32_t *packet_id)
{
	*generator_id = unique_id->generator_id;
	*packet_id = unique_id->packet_id;
}

struct task_base;

void task_gen_set_pkt_count(struct task_base *tbase, uint32_t count);
void task_gen_set_pkt_size(struct task_base *tbase, uint32_t pkt_size);
void task_gen_set_rate(struct task_base *tbase, uint64_t bps);
void task_gen_set_gateway_ip(struct task_base *tbase, uint32_t ip);
void task_gen_reset_randoms(struct task_base *tbase);
void task_gen_reset_values(struct task_base *tbase);
int task_gen_set_value(struct task_base *tbase, uint32_t value, uint32_t offset, uint32_t len);
int task_gen_add_rand(struct task_base *tbase, const char *rand_str, uint32_t offset, uint32_t rand_id);

uint32_t task_gen_get_n_randoms(struct task_base *tbase);
uint32_t task_gen_get_n_values(struct task_base *tbase);

#endif /* _HANDLE_GEN_H_ */
