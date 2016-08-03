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

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include <inttypes.h>

extern uint32_t rdtsc_overhead;
extern uint32_t rdtsc_overhead_stats;

void prox_init_tsc_overhead(void);

/* The precision of the conversion is nano-second. */
uint64_t str_to_tsc(const char *from);
uint64_t sec_to_tsc(uint64_t sec);
uint64_t msec_to_tsc(uint64_t msec);
uint64_t usec_to_tsc(uint64_t usec);
uint64_t nsec_to_tsc(uint64_t nsec);
uint64_t freq_to_tsc(uint64_t times_per_sec);

struct timeval;
void tsc_to_tv(struct timeval *tv, const uint64_t tsc);
void tv_to_tsc(const struct timeval *tv, uint64_t *tsc);
struct timeval tv_diff(const struct timeval *tv1, const struct timeval * tv2);

#endif /* _CLOCK_H_ */
