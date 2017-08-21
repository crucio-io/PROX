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

#ifndef _TOKEN_TIME_H_
#define _TOKEN_TIME_H_

#include <rte_cycles.h>
#include <math.h>

#include "prox_assert.h"

struct token_time_cfg {
	uint64_t bpp;
	uint64_t period;
	uint64_t bytes_max;
};

struct token_time {
	uint64_t tsc_last;
	uint64_t tsc_last_bytes;
	uint64_t bytes_now;
	struct token_time_cfg cfg;
};

/* Convert a given fractional bytes per period into bpp with as
   minimal loss of accuracy. */
static struct token_time_cfg token_time_cfg_create(double frac, uint64_t period, uint64_t bytes_max)
{
	struct token_time_cfg ret;

	/* Since period is expressed in units of cycles and it is in
	   most cases set to 1 second (which means its value is <=
	   3*10^9) and 2^64/10^9 > 6148914691 > 2^32). This means that
	   at most, period and frac will be doubled 32 times by the
	   following algorithm. Hence, the total error introduced by
	   the chosen values for bpp and period will be between 0 and
	   1/2^33. Note that since there are more operations that
	   can't overflow, the actual accuracy will probably be
	   lower. */

	/* The reason to limit period by UINT64_MAX/(uint64_t)frac is
	   that at run-time, the token_time_update function will
	   multiply a number that is <= period with bpp. In addition,
	   the token_time_tsc_until function will multiply at most
	   bytes_max with period so make sure that can't overflow. */

	while (period < UINT64_MAX/2 && frac != floor(frac) &&
	       (frac < 2.0f || period < UINT64_MAX/4/(uint64_t)frac) &&
	       (bytes_max == UINT64_MAX || period < UINT64_MAX/2/bytes_max)) {
		period *= 2;
		frac *= 2;
	}

	ret.bpp = floor(frac + 0.5);
	ret.period = period;
	ret.bytes_max = bytes_max;

	return ret;
}

static void token_time_update(struct token_time *tt, uint64_t tsc)
{
	uint64_t new_bytes;
	uint64_t t_diff = tsc - tt->tsc_last;

	/* Since the rate is expressed in tt->bpp, i.e. bytes per
	   period, counters can only be incremented/decremented
	   accurately every period cycles. */

	/* If the last update was more than a period ago, the update
	   can be performed accurately. */
	if (t_diff > tt->cfg.period) {
		/* First add remaining tokens in the last period that
		   was added partially. */
		new_bytes = tt->cfg.bpp - tt->tsc_last_bytes;
		tt->tsc_last_bytes = 0;
		tt->bytes_now += new_bytes;
		t_diff -= tt->cfg.period;
		tt->tsc_last += tt->cfg.period;

		/* If now it turns out that more periods have elapsed,
		   add the bytes for those periods directly. */
		if (t_diff > tt->cfg.period) {
			uint64_t periods = t_diff/tt->cfg.period;

			tt->bytes_now += periods * tt->cfg.bpp;
			t_diff -= tt->cfg.period * periods;
			tt->tsc_last += tt->cfg.period * periods;
		}
	}

	/* At this point, t_diff will be guaranteed to be less
	   than tt->cfg.period. */
	new_bytes = t_diff * tt->cfg.bpp/tt->cfg.period - tt->tsc_last_bytes;
	tt->tsc_last_bytes += new_bytes;
	tt->bytes_now += new_bytes;
	if (tt->bytes_now > tt->cfg.bytes_max)
		tt->bytes_now = tt->cfg.bytes_max;
}

static void token_time_set_bpp(struct token_time *tt, uint64_t bpp)
{
	tt->cfg.bpp = bpp;
}

static void token_time_init(struct token_time *tt, const struct token_time_cfg *cfg)
{
	tt->cfg = *cfg;
}

static void token_time_reset(struct token_time *tt, uint64_t tsc, uint64_t bytes_now)
{
	tt->tsc_last = tsc;
	tt->bytes_now = bytes_now;
	tt->tsc_last_bytes = 0;
}

static void token_time_reset_full(struct token_time *tt, uint64_t tsc)
{
	token_time_reset(tt, tsc, tt->cfg.bytes_max);
}

static int token_time_take(struct token_time *tt, uint64_t bytes)
{
	if (bytes > tt->bytes_now)
		return -1;
	tt->bytes_now -= bytes;
	return 0;
}

static void token_time_take_clamp(struct token_time *tt, uint64_t bytes)
{
	if (bytes > tt->bytes_now)
		tt->bytes_now = 0;
	else
		tt->bytes_now -= bytes;
}

static uint64_t token_time_tsc_until(const struct token_time *tt, uint64_t bytes)
{
	if (tt->bytes_now >= bytes)
		return 0;

	return (bytes - tt->bytes_now) * tt->cfg.period / tt->cfg.bpp;
}

static uint64_t token_time_tsc_until_full(const struct token_time *tt)
{
	return token_time_tsc_until(tt, tt->cfg.bytes_max);
}

#endif /* _TOKEN_TIME_H_ */
