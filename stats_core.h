/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2018 Viosoft Corporation.
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

#ifndef _STATS_CORE_H_
#define _STATS_CORE_H_

#include <inttypes.h>

struct lcore_stats_sample {
	uint64_t afreq;
	uint64_t mfreq;
	uint64_t mbm_tot_bytes;
	uint64_t mbm_loc_bytes;
};

struct lcore_stats {
	uint32_t lcore_id;
	uint32_t socket_id;
	uint64_t rmid;
	uint64_t cmt_data;
	uint64_t cmt_bytes;
	uint64_t mbm_tot_bytes;
	uint64_t mbm_loc_bytes;
	uint64_t cmt_fraction;
	uint32_t cat_mask;
	uint64_t mbm_tot;
	uint64_t mbm_loc;
	uint32_t class;
	struct lcore_stats_sample sample[2];
};

uint32_t stats_lcore_find_stat_id(uint32_t lcore_id);
int stats_get_n_lcore_stats(void);
struct lcore_stats *stats_get_lcore_stats(uint32_t stat_id);
struct lcore_stats_sample *stats_get_lcore_stats_sample(uint32_t stat_id, int last);
int stats_cpu_freq_enabled(void);
int stats_cmt_enabled(void);
int stats_cat_enabled(void);
int stats_mbm_enabled(void);
void stats_lcore_update(void);
void stats_lcore_init(void);
void stats_lcore_post_proc(void);
void stats_update_cache_mask(uint32_t lcore_id, uint32_t mask);
void stats_lcore_assoc_rmid(void);

#endif /* _STATS_CORE_H_ */
