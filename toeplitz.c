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

#include <stdio.h>
#include <stdint.h>
#include "toeplitz.h"

/* From XL710 Datasheet, 7.1.10 */

uint8_t toeplitz_init_key[TOEPLITZ_KEY_LEN] =
	{0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x8f, 0xb0,
	 0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	 0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	 0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	 0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00
};

uint32_t toeplitz_hash(uint8_t *buf_p, int buflen)
{
	uint32_t result = 0;
	uint8_t *key_p = toeplitz_init_key;
	uint8_t byte, *byte4_p = key_p+4;
	int i, pos = 0;
	int bit = 0;
	uint32_t key_word = __builtin_bswap32(*(uint32_t *)key_p);

	for (i = 0; i < buflen; ++i) {
		byte = buf_p[i];
		for (bit = 0; bit <= 7; ++bit) {
			if (byte & (1 << (7 - bit))) {
				result ^= key_word;
			}
			key_word = (key_word << 1) | ((*byte4_p >> (7 - bit)) & 1);
		}
		if (pos >= TOEPLITZ_KEY_LEN - 4) {
			pos = 0;
			byte4_p = key_p;
		}
		else {
			pos++;
			byte4_p++;
		}
	}
	return result;
}
