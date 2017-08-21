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

#include <fstream>
#include <arpa/inet.h>

#include "halfstream.hpp"

HalfStream::Action::Part HalfStream::addPkt(const PcapPkt &pkt)
{
	const uint32_t pktId = pkts.size();
	const uint8_t *l5;
	uint32_t l5Len;
	uint16_t tmpHdrLen;

	const struct PcapPkt::tcp_hdr *tcp;

	struct pkt_tuple pt = pkt.parsePkt((const uint8_t **)&tcp, &tmpHdrLen, &l5, &l5Len);

	if (pt.proto_id == IPPROTO_TCP) {
		if (tcp->tcp_flags & 0x02)
			tcpOpen = true;
		if (tcp->tcp_flags & 0x01)
			tcpClose = true;
	}

	if (pkts.empty()) {
		first = pkt.ts();
		hdrLen = tmpHdrLen;
		memcpy(hdr, pkt.payload(), hdrLen);
	}
	last = pkt.ts();
	totLen += pkt.len();
	contentLen += l5Len;

	pkts.push_back(pkt);

	return Action::Part(pktId, l5 - pkt.payload(), l5Len);
}

double HalfStream::getRate() const
{
	if (pkts.empty())
		return 0;
	if (first == last)
		return 1250000000;

	return totLen / (last - first);
}

HalfStream::Action::Action(HalfStream* stream, const Part &p, bool isClient)
	: halfStream(stream), m_isClient(isClient)
{
	addPart(p);
}

void HalfStream::Action::addPart(const Part &p)
{
	parts.push_back(p);
}

uint32_t HalfStream::Action::totLen() const
{
	uint32_t ret = 0;

	for (list<Part>::const_iterator i = parts.begin(); i != parts.end(); ++i) {
		ret += (*i).len;
	}

	return ret;
}

void HalfStream::Action::toFile(ofstream *f) const
{
	for (list<Part>::const_iterator i = parts.begin(); i != parts.end(); ++i) {
		const PcapPkt &pkt = halfStream->pkts[i->pktId];
		const uint8_t *payload = &pkt.payload()[i->offset];
		const uint16_t len = i->len;

		f->write((const char *)payload, len);
	}
}

HalfStream::HalfStream()
	: first(0, 0), last(0, 0), totLen(0), hdrLen(0), contentLen(0), tcpOpen(false), tcpClose(false)
{

}
