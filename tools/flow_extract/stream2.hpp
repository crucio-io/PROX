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

#ifndef _STREAM2_H_
#define _STREAM2_H_

#include <inttypes.h>
#include <fstream>

#include "netsocket.hpp"

using namespace std;

class Stream2 {
public:
	Stream2() : clientServerHdrContent(NULL) {}
	~Stream2() {delete [] clientServerHdrContent;}
	int fromFile(ifstream *f);
	void calcOffsets(ofstream *out);
	void toFile(ofstream *out) const;
	void toLua(ofstream *f, const string& binFileName, const string& streamTableName) const;
	NetSocket getServerNetSocket() const;
	NetSocket getClientNetSocket() const;
	void setServerNetSocket(const NetSocket& netSocket);
	void setClientNetSocket(const NetSocket& netSocket);
	Stream::Header      streamHdr;
private:
	int actionsFromFile(ifstream *f, size_t actionCount);
	int setReferences(ifstream *f);

	uint8_t *clientServerHdrContent;

	uint32_t clientHdrBeg;
	uint32_t serverHdrBeg;
	uint32_t clientContentBeg;
	uint32_t serverContentBeg;

	vector<Stream::ActionEntry> m_actions;
};

#endif /* _STREAM2_H_ */
