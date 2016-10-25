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

#ifndef _ELD_H_
#define _ELD_H_

#define PACKET_QUEUE_BITS      14
#define PACKET_QUEUE_SIZE      (1 << PACKET_QUEUE_BITS)
#define PACKET_QUEUE_MASK      (PACKET_QUEUE_SIZE - 1)

#define QUEUE_ID_BITS		(32 - PACKET_QUEUE_BITS)
#define QUEUE_ID_SIZE		(1 << QUEUE_ID_BITS)
#define QUEUE_ID_MASK		(QUEUE_ID_SIZE - 1)

struct early_loss_detect {
	uint32_t entries[PACKET_QUEUE_SIZE];
	uint32_t last_pkt_idx;
};

static void early_loss_detect_reset(struct early_loss_detect *eld)
{
	for (size_t i = 0; i < PACKET_QUEUE_SIZE; i++) {
		eld->entries[i] = -1;
	}
}

static uint32_t early_loss_detect_count_remaining_loss(struct early_loss_detect *eld)
{
	uint32_t queue_id;
	uint32_t n_loss;
	uint32_t n_loss_total = 0;

	/* Need to check if we lost any packet before last packet
	   received Any packet lost AFTER the last packet received
	   cannot be counted.  Such a packet will be counted after both
	   lat and gen restarted */
	queue_id = eld->last_pkt_idx >> PACKET_QUEUE_BITS;
	for (uint32_t i = (eld->last_pkt_idx + 1) & PACKET_QUEUE_MASK; i < PACKET_QUEUE_SIZE; i++) {
		// We ** might ** have received OOO packets; do not count them as lost next time...
		if (queue_id - eld->entries[i] != 0) {
			n_loss = (queue_id - eld->entries[i] - 1) & QUEUE_ID_MASK;
			n_loss_total += n_loss;
		}
	}
	for (uint32_t i = 0; i < (eld->last_pkt_idx & PACKET_QUEUE_MASK); i++) {
		// We ** might ** have received OOO packets; do not count them as lost next time...
		if (eld->entries[i] - queue_id != 1) {
			n_loss = (queue_id - eld->entries[i]) & QUEUE_ID_MASK;
			n_loss_total += n_loss;
		}
	}

	eld->entries[eld->last_pkt_idx & PACKET_QUEUE_MASK] = -1;
	return n_loss_total;
}

static uint32_t early_loss_detect_add(struct early_loss_detect *eld, uint32_t packet_index)
{
	uint32_t old_queue_id, queue_pos, n_loss;

	eld->last_pkt_idx = packet_index;
	queue_pos = packet_index & PACKET_QUEUE_MASK;
	old_queue_id = eld->entries[queue_pos];
	eld->entries[queue_pos] = packet_index >> PACKET_QUEUE_BITS;

	return (eld->entries[queue_pos] - old_queue_id - 1) & QUEUE_ID_MASK;
}

#endif /* _ELD_H_ */
