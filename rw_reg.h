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

#ifndef __RW_REG_H__
#define __RW_REG_H__

/* Simplified, from DPDK 1.8 */
struct _dev_hw {
	uint8_t *hw_addr;
};
/* Registers access */

#define PROX_PCI_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr + (reg)))
#define PROX_READ_REG(hw, reg) \
	prox_read_addr(PROX_PCI_REG_ADDR((hw), (reg)))
#define PROX_PCI_REG(reg) (*((volatile uint32_t *)(reg)))
#define PROX_PCI_REG_WRITE(reg_addr, value) \
	*((volatile uint32_t *) (reg_addr)) = (value)
#define PROX_WRITE_REG(hw,reg,value) \
	PROX_PCI_REG_WRITE(PROX_PCI_REG_ADDR((hw), (reg)), (value))

static inline uint32_t prox_read_addr(volatile void* addr)
{
        return rte_le_to_cpu_32(PROX_PCI_REG(addr));
}

int read_reg(uint8_t portid, uint32_t addr, uint32_t *reg);
int write_reg(uint8_t portid, uint32_t reg, uint32_t val);
#endif
