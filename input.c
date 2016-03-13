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

#include <rte_cycles.h>
#include <rte_common.h>

#include "clock.h"
#include "input.h"

static struct input *inputs[32];
static int n_inputs;
static int max_input_fd;

int reg_input(struct input *in)
{
	if (n_inputs == sizeof(inputs)/sizeof(inputs[0]))
		return -1;

	for (int i = 0; i < n_inputs; ++i) {
		if (inputs[i] == in)
			return -1;
	}
	inputs[n_inputs++] = in;
	max_input_fd = RTE_MAX(in->fd, max_input_fd);

	return 0;
}

void unreg_input(struct input *in)
{
	int rm, i;

	for (rm = 0; rm < n_inputs; ++rm) {
		if (inputs[rm] == in) {
			break;
		}
	}

	if (rm == n_inputs)
		return ;

	for (i = rm + 1; i < n_inputs; ++i) {
		inputs[i - 1] = inputs[i];
	}

	n_inputs--;
	max_input_fd = 0;
	for (i = 0; i < n_inputs; ++i) {
		max_input_fd = RTE_MAX(inputs[i]->fd, max_input_fd);
	}
}

static int tsc_diff_to_tv(uint64_t beg, uint64_t end, struct timeval *tv)
{
	if (end < beg) {
		return -1;
	}

	uint64_t diff = end - beg;
	tsc_to_tv(tv, diff);
	return 0;
}

void input_proc_until(uint64_t deadline)
{
	struct timeval tv;
	fd_set in_fd;
	int ret = 1;

	/* Keep checking for input until select() returned 0 (timeout
	   occurred before input was read) or current time has passed
	   the deadline (which occurs when time progresses past the
	   deadline between return of select() and the next
	   iteration). */
	while (ret != 0 && tsc_diff_to_tv(rte_rdtsc(), deadline, &tv) == 0) {
		FD_ZERO(&in_fd);

		for (int i = 0; i < n_inputs; ++i) {
			FD_SET(inputs[i]->fd, &in_fd);
		}

		ret = select(max_input_fd + 1, &in_fd, NULL, NULL, &tv);

		if (ret > 0) {
			for (int i = 0; i < n_inputs; ++i) {
				if (FD_ISSET(inputs[i]->fd, &in_fd)) {
					inputs[i]->proc_input(inputs[i]);
				}
			}
		}
	}
}
