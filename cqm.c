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

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "msr.h"
#include "cqm.h"

#define IA32_QM_EVTSEL	3213
#define IA32_QM_CTR	3214
#define IA32_QM_ASSOC	3215

static struct cqm_features cqm_features;
static int stat_core;

struct reg {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

static void cpuid(struct reg* r, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	asm volatile("cpuid"
		     : "=a" (r->eax), "=b" (r->ebx), "=c" (r->ecx), "=d" (r->edx)
		     : "a" (a), "b" (b), "c" (c), "d" (d));
}

int cqm_is_supported(void)
{
	struct reg r;

	cpuid(&r, 0x7, 0x0, 0x0, 0x0);

	if (!((r.ebx >> 12) & 1)) {
		return 0;
	}

	cpuid(&r, 0xf, 0x0, 0x0, 0x0);

	/* Check if L3 QoS Monitoring capability is present. */
	if (!((r.edx >> 1) & 1)) {
		return 0;
	}

	cpuid(&r, 0xf, 0x0, 0x1, 0x0);


	cqm_features.upscaling_factor = r.ebx;
	cqm_features.max_rmid = r.ecx;
	cqm_features.event_types = r.edx;

	return 1;
}

int cqm_get_features(struct cqm_features* feat)
{
	if (!cqm_is_supported())
		return 1;

	*feat = cqm_features;
	return 0;
}

int cqm_assoc(uint8_t lcore_id, uint64_t rmid)
{
	return msr_write(lcore_id, rmid, IA32_QM_ASSOC);
}

void cqm_init_stat_core(uint8_t lcore_id)
{
	stat_core = lcore_id;
}

/* read a specific rmid value using core 0 */
int cqm_read_ctr(uint64_t* ret, uint64_t rmid)
{
	uint64_t event_id = 1;

	uint64_t es = rmid;
	es = (es << 32) | event_id;

	if (msr_write(stat_core, es, IA32_QM_EVTSEL) < 0) {
		return 1;
	}

	if (msr_read(ret, stat_core, IA32_QM_CTR) < 0) {
		return 2;
	}

	return 0;
}
