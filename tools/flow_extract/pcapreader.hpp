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

#ifndef _PCAPREADER_H_
#define _PCAPREADER_H_

#include <inttypes.h>
#include <string>

#include <pcap.h>

#include "pcappkt.hpp"

using namespace std;

class PcapReader {
public:
        PcapReader() : m_handle(NULL), pktReadCount(0) {}
	int open(const string& file_path);
	size_t pos() {return ftell(pcap_file(m_handle)) - m_file_beg;}
	size_t end() {return m_file_end;}
	int read(PcapPkt *pkt);
	int readOnce(PcapPkt *pkt, uint64_t pos);
	size_t getPktReadCount() const {return pktReadCount;}
	void close();
	const string &getError() const {return m_error;}
private:
	pcap_t *m_handle;
	size_t m_file_beg;
	size_t m_file_end;
	size_t pktReadCount;
	string m_error;
};

#endif /* _PCAPREADER_H_ */
