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

#include <stdio.h>

#include "dpi.h"

/* The following functions are not a real implementation of a
   DPI. They serve only to create dpi_stub.so which can be loaded into
   prox. */

static int dpi_init(uint32_t thread_count, int argc, const char *argv[])
{
	return 0;
}

size_t dpi_get_flow_entry_size(void) {return 0;}
void flow_data_dpi_flow_expire(void *flow_data) {}
void *dpi_thread_start() {return NULL;}
void dpi_thread_stop(void *opaque) {}
void dpi_finish(void) {}

int dpi_process(void *opaque, struct flow_info *fi, void *flow_data,
		struct dpi_payload *payload, uint32_t results[],
		size_t *result_len)
{
	return 0;
}

static struct dpi_engine dpi_engine = {
	.dpi_init = dpi_init,
	.dpi_get_flow_entry_size = dpi_get_flow_entry_size,
	.dpi_flow_expire = flow_data_dpi_flow_expire,
	.dpi_thread_start = dpi_thread_start,
	.dpi_thread_stop = dpi_thread_stop,
	.dpi_process = dpi_process,
	.dpi_finish = dpi_finish,
	.dpi_print = printf,
};

struct dpi_engine *get_dpi_engine(void)
{
	return &dpi_engine;
}
