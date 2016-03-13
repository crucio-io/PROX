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

#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define USEHP

using namespace std;

#include "allocator.hpp"

Allocator::Allocator(size_t size, size_t threshold)
	: m_size(size), m_threshold(threshold), m_alloc_offset(0)
{
#ifdef USEHP
	int fd = open("/mnt/huge/hp", O_CREAT | O_RDWR, 0755);
	if (fd < 0) {
		cerr << "Allocator failed to open huge page file descriptor: " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}
	m_mem = (uint8_t *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (m_mem == MAP_FAILED) {
		perror("mmap");
		unlink("/mnt/huge");
		cerr << "Allocator mmap failed: " << strerror(errno) << endl;
		exit (EXIT_FAILURE);
	}
#else
	m_mem = new uint8_t[size];
#endif
}

Allocator::~Allocator()
{
#ifdef USEHP
	munmap((void *)m_mem, m_size);
#else
	delete[] m_mem;
#endif
}

void *Allocator::alloc(size_t size)
{
	void *ret = &m_mem[m_alloc_offset];

	m_alloc_offset += size;
	return ret;
}

void Allocator::reset()
{
	m_alloc_offset = 0;
}

size_t Allocator::getFreeSize() const
{
	return m_size - m_alloc_offset;
}

bool Allocator::lowThresholdReached() const
{
	return (m_size - m_alloc_offset) < m_threshold;
}
