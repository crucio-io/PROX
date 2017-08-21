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

#ifndef _STREAM3_H_
#define _STREAM3_H_

#include <inttypes.h>
#include <vector>

#include "pcappkt.hpp"
#include "timestamp.hpp"

using namespace std;
class Allocator;

class Stream3 {
public:
	PcapPkt::L4Proto getProto(void) const {return m_proto;}
	Stream3(uint32_t id, PcapPkt::L4Proto proto);
	Stream3() : m_id(UINT32_MAX), m_proto(PcapPkt::PROTO_UDP), m_pktCount(0), m_flushCount(0) {}
	void addPkt(const PcapPkt& pkt);
	void flush(ofstream *outputFile);
	void addFromMemory(uint8_t *mem, size_t *len);
	static uint32_t getIDFromMem(uint8_t *mem);
	bool hasFlushablePackets() const {return !!m_flushCount;}
	Timestamp getTimeout() const;
	uint32_t getID() const {return m_id;}
	void removeAllPackets();
	void setID(const uint32_t id) {m_id = id;}
private:
	void writeHeader(ofstream *outputFile) const;
	void writePackets(ofstream *outputFile) const;
	void clearPackets();

	uint32_t m_id;
	PcapPkt::L4Proto m_proto;
	vector<PcapPkt *> m_pkts;
	uint32_t m_pktCount;
	uint32_t m_flushCount;
};

#endif /* _STREAM3_H_ */
