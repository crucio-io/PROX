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

#ifndef _CQM_H_
#define _CQM_H_

#include <inttypes.h>
#include <stdio.h>

#define PROX_MAX_CACHE_SET      16

struct rdt_features {
	uint8_t rdtm_supported;
	uint8_t rdta_supported;
	uint8_t cmt_supported;
	uint8_t	mbm_tot_supported;
	uint8_t	mbm_loc_supported;
	uint8_t l3_cat_supported;
	uint8_t l2_cat_supported;
	uint8_t mba_supported;
	uint32_t rdtm_max_rmid;
	uint32_t cmt_max_rmid;
	uint32_t cat_max_rmid;
	uint32_t mba_max_rmid;
	uint32_t cat_num_ways;
	uint32_t upscaling_factor;
	uint32_t event_types;
};

struct prox_cache_set_cfg {
	uint32_t mask;
	uint32_t lcore_id;
	int32_t socket_id;
};

int rdt_is_supported(void);
int cmt_is_supported(void);
int cat_is_supported(void);
int mbm_is_supported(void);
int mba_is_supported(void);

int rdt_get_features(struct rdt_features* feat);

int cqm_assoc(uint8_t lcore_id, uint64_t rmid);
int cqm_assoc_read(uint8_t lcore_id, uint64_t *rmid);

void rdt_init_stat_core(uint8_t lcore_id);

int cmt_read_ctr(uint64_t* ret, uint64_t rmid, uint8_t lcore_id);
int mbm_read_tot_bdw(uint64_t* ret, uint64_t rmid, uint8_t lcore_id);
int mbm_read_loc_bdw(uint64_t* ret, uint64_t rmid, uint8_t lcore_id);
void read_rdt_info(void);
extern struct prox_cache_set_cfg prox_cache_set_cfg[PROX_MAX_CACHE_SET];
int cat_log_init(uint8_t lcore_id);
int cat_set_class_mask(uint8_t lcore_id,  uint32_t set, uint32_t mask);
int cat_get_class_mask(uint8_t lcore_id, uint32_t set, uint32_t *mask);
void cat_reset_cache(uint32_t lcore_id);
int cat_get_num_ways(void);

#endif /* _CQM_H_ */
