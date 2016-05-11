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

#ifndef _STREAMSORTER_H_
#define _STREAMSORTER_H_

#include "stream3.hpp"
#include "flowtable.hpp"
#include "allocator.hpp"

class StreamSorter {
public:
	StreamSorter(size_t flowTableSize, const string& workingDirectory, size_t memoryLimit);
	void sort(const string &inputPcapFile, const string &outputBinFile);
private:
	void sortChunks(const string &inputPcapFilePath);
	void mergeChunks(const string &outputBinFilePath);
	void setTempFileName();
	void processPkt(const PcapPkt &pkt);
	void resetStreams();
	FlowTable<pkt_tuple, uint32_t>::entry* getFlowEntry(const PcapPkt &pkt);
	void flushStreams(ofstream *outputTempFile);
	Stream3 *addNewStream(PcapPkt::L4Proto proto);
	size_t flowTableSize;
	FlowTable<pkt_tuple, uint32_t> *ft;
	vector<size_t> flushOffsets;
	vector<Stream3> streams;
	string tempFilePath;
	const string workingDirectory;
	Allocator allocator;
	uint32_t streamID;
};

#endif /* _STREAMSORTER_H_ */
