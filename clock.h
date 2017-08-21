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

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include <inttypes.h>

extern uint32_t rdtsc_overhead;
extern uint32_t rdtsc_overhead_stats;

void clock_init(void);

struct time_unit {
	uint64_t sec;
	uint64_t nsec;
};

struct time_unit_err {
	struct time_unit time;
	struct time_unit error;
};

extern uint64_t thresh;
extern uint64_t tsc_hz;

static uint64_t val_to_rate(uint64_t val, uint64_t delta_t)
{
	if (val < thresh) {
		return val * tsc_hz / delta_t;
	} else if (val >> 2 < thresh) {
		/* bytes per sec malls into this category ... */
		return ((val >> 2) * tsc_hz) / (delta_t >> 2);
	} else {
		if (delta_t < tsc_hz)
			return UINT64_MAX;
		else
			return val / (delta_t/tsc_hz);
	}
}

/* The precision of the conversion is nano-second. */
uint64_t str_to_tsc(const char *from);
uint64_t sec_to_tsc(uint64_t sec);
uint64_t msec_to_tsc(uint64_t msec);
uint64_t usec_to_tsc(uint64_t usec);
uint64_t nsec_to_tsc(uint64_t nsec);
uint64_t freq_to_tsc(uint64_t times_per_sec);
uint64_t tsc_to_msec(uint64_t tsc);
uint64_t tsc_to_usec(uint64_t tsc);
uint64_t tsc_to_nsec(uint64_t tsc);
uint64_t tsc_to_sec(uint64_t tsc);
struct time_unit tsc_to_time_unit(uint64_t tsc);
uint64_t time_unit_to_usec(struct time_unit *time_unit);
uint64_t time_unit_to_nsec(struct time_unit *time_unit);
int time_unit_cmp(struct time_unit *left, struct time_unit *right);

struct timeval;
void tsc_to_tv(struct timeval *tv, const uint64_t tsc);
void tv_to_tsc(const struct timeval *tv, uint64_t *tsc);
struct timeval tv_diff(const struct timeval *tv1, const struct timeval * tv2);

#endif /* _CLOCK_H_ */
