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

/*
  This pseudorandom number generator is based on ref_xorshift128plus,
  as implemented by reference_xorshift.h, which has been obtained
  from https://sourceforge.net/projects/xorshift-cpp/

  The licensing terms for reference_xorshift.h are reproduced below.

  //  Written in 2014 by Ivo Doko (ivo.doko@gmail.com)
  //  based on code written by Sebastiano Vigna (vigna@acm.org)
  //  To the extent possible under law, the author has dedicated
  //  all copyright and related and neighboring rights to this
  //  software to the public domain worldwide. This software is
  //  distributed without any warranty.
  //  See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <rte_cycles.h>

struct random {
  uint64_t state[2];
};

static void random_init_seed(struct random *random)
{
  random->state[0] = rte_rdtsc();
  random->state[1] = rte_rdtsc();
}

static uint64_t random_next(struct random *random)
{
  const uint64_t s0 = random->state[1];
  const uint64_t s1 = random->state[0] ^ (random->state[0] << 23);

  random->state[0] = random->state[1];
  random->state[1] = (s1 ^ (s1 >> 18) ^ s0 ^ (s0 >> 5)) + s0;
  return random->state[1];
}

#endif /* _RANDOM_H_ */
