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

#ifndef _DPI_H_
#define _DPI_H_

#include <sys/time.h>
#include <inttypes.h>
#include <stddef.h>

struct flow_info {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint8_t ip_proto;
	uint16_t port_src;
	uint16_t port_dst;
	uint8_t reservered[3];
} __attribute__((packed));

struct dpi_payload {
	uint8_t  *payload;
	uint16_t len;
	uint16_t client_to_server;
	struct timeval tv;
};

struct dpi_engine {
	/* Returns 0 on success, This function is called from an
	   arbitrary thread before any other function in this struct
	   is called. */
	int (*dpi_init)(uint32_t thread_count, int argc, const char *argv[]);
	/* Return the size that should be allocated in the flow
	   table. It is the sizeof(*flow_data) passed to
	   dpi_process(). */
	size_t (*dpi_get_flow_entry_size)(void);
	/* Called before the flow entry is expired. */
	void (*dpi_flow_expire)(void *flow_data);
	/* start function called from a DPI thread itself. The opaque
	   pointer returned here will be passed to dpi_thread_stop and
	   dpi_process. */
	void *(*dpi_thread_start)(void);
	/* Stop function called from a DPI thread itself. */
	void (*dpi_thread_stop)(void *opaque);
	/* Processing function to perform actual DPI work. struct
	   flow_info contains the 5 tuple, flow_data is the entry in
	   the flow table which has a size specified by
	   dpi_get_flow_entry_size(). The payload (together with the
	   time and the direction) is passed through the payload
	   parameter. DPI results are returned by the results
	   array. The function returns 0 on success. */
	int (*dpi_process)(void *opaque, struct flow_info *fi, void *flow_data,
			   struct dpi_payload *payload, uint32_t results[],
			   size_t *result_len);
	/* Called once at cleanup. */
	void (*dpi_finish)(void);
	/* Function used for printing. */
	int (*dpi_print)(const char *fmt, ...);
};

/* Returns the implementation of a dpi_engine. */
struct dpi_engine *get_dpi_engine(void);

#endif /* _DPI_H_ */
