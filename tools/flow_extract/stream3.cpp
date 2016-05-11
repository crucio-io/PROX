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
#include <iostream>
#include <fstream>

using namespace std;

#include "stream3.hpp"

Stream3::Stream3(uint32_t id, PcapPkt::L4Proto proto)
	: m_id(id), m_proto(proto), m_pktCount(0), m_flushCount(0)
{
}

void Stream3::writeHeader(ofstream *outputFile) const
{
	outputFile->write(reinterpret_cast<const char *>(&m_id), sizeof(m_id));
	outputFile->write(reinterpret_cast<const char *>(&m_flushCount), sizeof(m_flushCount));
}

void Stream3::writePackets(ofstream *outputFile) const
{
	for (size_t i  = 0; i < m_pkts.size(); ++i)
		m_pkts[i]->toFile(outputFile);
}

void Stream3::clearPackets()
{
	for (size_t i = 0; i < m_pkts.size(); ++i)
		delete m_pkts[i];
	m_pkts.clear();
	m_flushCount = 0;
}

void Stream3::flush(ofstream *outputFile)
{
	writeHeader(outputFile);
	writePackets(outputFile);
	clearPackets();
}

void Stream3::addPkt(const PcapPkt& pkt)
{
	m_pkts.push_back(new PcapPkt(pkt));
	m_pktCount++;
	m_flushCount++;
}

Timestamp Stream3::getTimeout() const
{
	uint32_t timeoutMinutes = m_proto == PcapPkt::PROTO_UDP? 10 : 5;

	return Timestamp(timeoutMinutes * 60, 0);
}

uint32_t Stream3::getIDFromMem(uint8_t *mem)
{
	return *reinterpret_cast<uint32_t *>(mem);
}

void Stream3::addFromMemory(uint8_t *mem, size_t *len)
{
	uint32_t n_pkts;

	mem += sizeof(m_id);
	n_pkts = *reinterpret_cast<uint32_t *>(mem);
	mem += sizeof(n_pkts);

	*len = sizeof(m_id) + sizeof(n_pkts);
	for (uint32_t i = 0; i < n_pkts; ++i) {
	        addPkt(PcapPkt(mem));
		mem += m_pkts.back()->memSize();
		*len += m_pkts.back()->memSize();
	}
}

void Stream3::removeAllPackets()
{
	clearPackets();
	m_pktCount = 0;
}
