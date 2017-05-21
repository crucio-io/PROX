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

#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "msr.h"
#include "log.h"

int msr_fd[RTE_MAX_LCORE];
int n_msr_fd;
int msr_init(void)
{
	char msr_path[1024];

	if (n_msr_fd) {
		return 0;
	}

	for (uint32_t i = 0; i < sizeof(msr_fd)/sizeof(*msr_fd); ++i, n_msr_fd = i) {
		snprintf(msr_path, sizeof(msr_path), "/dev/cpu/%u/msr", i);
		msr_fd[i] = open(msr_path, O_RDWR);
		if (msr_fd[i] < 0) {
			return i == 0? -1 : 0;
		}
	}

	return 0;
}

void msr_cleanup(void)
{
	for (int i = 0; i < n_msr_fd; ++i) {
		close(msr_fd[i]);
	}

	n_msr_fd = 0;
}

int msr_read(uint64_t *ret, int lcore_id, off_t offset)
{
	if (lcore_id > n_msr_fd) {
		return -1;
	}

	if (0 > pread(msr_fd[lcore_id], ret, sizeof(uint64_t), offset)) {
		return -1;
	}

	return 0;
}

int msr_write(int lcore_id, uint64_t val, off_t offset)
{
	if (lcore_id > n_msr_fd) {
		return -1;
	}

	if (sizeof(uint64_t) != pwrite(msr_fd[lcore_id], &val, sizeof(uint64_t), offset)) {
		return -1;
	}
	plog_dbg("\t\tmsr_write(core %d, offset %x, val %lx)\n", lcore_id, (int)offset, val);
	return 0;
}
