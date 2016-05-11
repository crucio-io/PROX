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

#ifndef _STREAM_H_
#define _STREAM_H_

#include <list>
#include <string>
#include <fstream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <sys/time.h>

#include "pcappktref.hpp"
#include "pcappkt.hpp"
#include "netsocket.hpp"
#include "timestamp.hpp"
#include "halfstream.hpp"

using namespace std;

class PcapReader;

class Stream {
public:
	struct Header {
		uint32_t streamId;
		uint16_t clientHdrLen;
		uint32_t clientContentLen;
		uint16_t serverHdrLen;
		uint32_t serverContentLen;
		uint32_t actionCount;
		uint32_t clientIP;
		uint16_t clientPort;
		uint32_t serverIP;
		uint16_t serverPort;
		double   upRate;
		double   dnRate;
		uint8_t  protocol;
		uint8_t  completedTCP;
 		void     toFile(ofstream *f) const;
		int      fromFile(ifstream *f);
		size_t   getStreamLen() const;
	};
	struct ActionEntry {
		uint8_t peer;
		uint32_t beg;
		uint32_t len;
	} __attribute__((packed));

	Stream(uint32_t id = -1, uint32_t sizeHint = 0);
	void addPkt(const PcapPkt &pkt);
	void toFile(ofstream *f);
	void toPcap(const string& outFile);
	double getRate() const;
	size_t actionCount() const {return m_actions.size();}

private:
	Header getHeader() const;
	void actionsToFile(ofstream *f) const;
	void clientHdrToFile(ofstream *f) const;
	void serverHdrToFile(ofstream *f) const;
	void contentsToFile(ofstream *f, bool isClient) const;
	bool isClient(const PcapPkt &pkt) const;
	size_t pktCount() const;
	struct pkt_tuple m_pt;
	void setTupleFromPkt(const PcapPkt &pkt);
	void addToClient(const PcapPkt &pkt);
	void addToServer(const PcapPkt &pkt);
	void addAction(HalfStream *half, HalfStream::Action::Part p, bool isClientPkt);

	int m_id;
	vector<PcapPkt> m_pkts;
	vector<HalfStream::Action> m_actions;
	HalfStream m_client;
	HalfStream m_server;
	bool m_prevPktIsClient;
};

#endif /* _STREAM_H_ */
