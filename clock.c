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

#include "clock.h"

#include <stdio.h>
#include <string.h>

#include <rte_cycles.h>

/* Calibrate TSC overhead by reading NB_READ times and take the smallest value.
   Bigger values are caused by external influence and can be discarded. The best
   estimate is the smallest read value. */
#define NB_READ 10000

uint32_t rdtsc_overhead;
uint32_t rdtsc_overhead_stats;

/* calculate how much overhead is involved with calling rdtsc. This value has
   to be taken into account where the time spent running a small piece of code
   is measured */
void prox_init_tsc_overhead(void)
{
	volatile uint32_t min_without_overhead = UINT32_MAX;
	volatile uint32_t min_with_overhead = UINT32_MAX;
	volatile uint32_t min_stats_overhead = UINT32_MAX;
	volatile uint64_t start1, end1;
	volatile uint64_t start2, end2;

	for (uint32_t i = 0; i < NB_READ; ++i) {
		start1 = rte_rdtsc();
		end1   = rte_rdtsc();

		start2 = rte_rdtsc();
		end2   = rte_rdtsc();
		end2   = rte_rdtsc();

		if (min_without_overhead > end1 - start1) {
			min_without_overhead = end1 - start1;
		}

		if (min_with_overhead > end2 - start2) {
			min_with_overhead = end2 - start2;
		}
	}

	rdtsc_overhead = min_with_overhead - min_without_overhead;


	start1 = rte_rdtsc();
	end1   = rte_rdtsc();
	/* forbid the compiler to optimize this dummy variable */
	volatile int dummy = 0;
	for (uint32_t i = 0; i < NB_READ; ++i) {
		start1 = rte_rdtsc();
		dummy += 32;
		end1   = rte_rdtsc();

		if (min_stats_overhead > end2 - start2) {
			min_stats_overhead = end1 - start1;
		}
	}

	rdtsc_overhead_stats = rdtsc_overhead + min_stats_overhead - min_without_overhead;
}

uint64_t str_to_tsc(const char *from)
{
	const uint64_t hz = rte_get_tsc_hz();
	uint64_t ret;
	char str[16];

	strncpy(str, from, sizeof(str));

	char *frac = strchr(str, '.');

	if (frac) {
		*frac = 0;
		frac++;
	}

	ret = hz * atoi(str);

	if (!frac)
		return ret;

	uint64_t nsec = 0;
	uint64_t multiplier = 100000000;

	for (size_t i = 0; i < strlen(frac); ++i) {
		nsec += (frac[i] - '0') * multiplier;
		multiplier /= 10;
	}

	/* Wont overflow until CPU freq is ~18.44 GHz */
	ret += hz * nsec/1000000000;

	return ret;
}

uint64_t sec_to_tsc(uint64_t sec)
{
	if (sec < UINT64_MAX/rte_get_tsc_hz())
		return sec * rte_get_tsc_hz();
	else
		return UINT64_MAX;
}

uint64_t msec_to_tsc(uint64_t msec)
{
	if (msec < UINT64_MAX/rte_get_tsc_hz())
		return msec * rte_get_tsc_hz() / 1000;
	else
		return msec / 1000 * rte_get_tsc_hz();
}

uint64_t usec_to_tsc(uint64_t usec)
{
	if (usec < UINT64_MAX/rte_get_tsc_hz())
		return usec * rte_get_tsc_hz() / 1000000;
	else
		return usec / 1000000 * rte_get_tsc_hz();
}

uint64_t nsec_to_tsc(uint64_t nsec)
{
	if (nsec < UINT64_MAX/rte_get_tsc_hz())
		return nsec * rte_get_tsc_hz() / 1000000000;
	else
		return nsec / 1000000000 * rte_get_tsc_hz();
}

void tsc_to_tv(struct timeval *tv, const uint64_t tsc)
{
	uint64_t hz = rte_get_tsc_hz();
	uint64_t sec = tsc/hz;

	tv->tv_sec = sec;
	tv->tv_usec = ((tsc - sec * hz) * 1000000) / hz;
}

void tv_to_tsc(const struct timeval *tv, uint64_t *tsc)
{
	uint64_t hz = rte_get_tsc_hz();
	*tsc = tv->tv_sec * hz;
	*tsc += tv->tv_usec * hz / 1000000;
}

struct timeval tv_diff(const struct timeval *cur, const struct timeval *next)
{
	uint64_t sec, usec;

	sec = next->tv_sec - cur->tv_sec;
	if (next->tv_usec < cur->tv_usec) {
		usec = next->tv_usec + 1000000 - cur->tv_usec;
		sec -= 1;
	}
	else
		usec = next->tv_usec - cur->tv_usec;

	struct timeval ret = {
		.tv_sec  = sec,
		.tv_usec = usec,
	};

	return ret;
}
