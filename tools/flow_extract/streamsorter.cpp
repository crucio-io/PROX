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

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "mappedfile.hpp"
#include "memreader.hpp"
#include "streamsorter.hpp"
#include "path.hpp"
#include "allocator.hpp"
#include "pcapreader.hpp"
#include "progress.hpp"

StreamSorter::StreamSorter(size_t flowTableSize, const string& workingDirectory, size_t memoryLimit)
	: flowTableSize(flowTableSize),
	  workingDirectory(workingDirectory),
	  allocator(memoryLimit, 1024*10),
	  streamID(0)
{
}

void StreamSorter::sort(const string &inputPcapFilePath, const string &outputBinFilePath)
{
	setTempFileName();
	sortChunks(inputPcapFilePath);
	mergeChunks(outputBinFilePath);
}

void StreamSorter::sortChunks(const string &inputPcapFilePath)
{
	ofstream outputTempFile;

	outputTempFile.open(tempFilePath.c_str());

	if (!outputTempFile.is_open())
		return ;

	PcapReader pr;
	PcapPkt pkt;

	if (pr.open(inputPcapFilePath)) {
		pr.getError();
		return;
	}
	PcapPkt::allocator = &allocator;

	Progress progress(pr.end());
	uint32_t packetDetail = progress.addDetail("packet count");

	ft = new FlowTable<pkt_tuple, uint32_t>(flowTableSize);
	resetStreams();

	while (pr.read(&pkt)) {
		processPkt(pkt);
		if (progress.couldRefresh()) {
			progress.setProgress(pr.pos());
			progress.setDetail(packetDetail, pr.getPktReadCount());
			progress.refresh();
		}
		if (allocator.lowThresholdReached()) {
			flushStreams(&outputTempFile);
		}
	}
	progress.setProgress();
	progress.setDetail(packetDetail, pr.getPktReadCount());
	progress.refresh(true);

	pr.close();
	flushStreams(&outputTempFile);
	PcapPkt::allocator = NULL;
	outputTempFile.close();
	delete ft;
}

void StreamSorter::resetStreams()
{
	streams.clear();
}

void StreamSorter::flushStreams(ofstream *outputTempFile)
{
	size_t flushCount = 0;
	size_t offset = outputTempFile->tellp();

	Progress progress(streams.size());

	cout << endl;
	progress.setTitle("flush ");
	for (size_t i = 0; i < streams.size(); ++i) {
		if (streams[i].hasFlushablePackets()) {
			streams[i].flush(outputTempFile);
			flushCount++;
		}

		if (progress.couldRefresh()) {
			progress.setProgress(i);
			progress.refresh();
		}
	}
	progress.setProgress();
	progress.refresh(true);

	if (flushCount)
		flushOffsets.push_back(offset);
	allocator.reset();
}

Stream3 *StreamSorter::addNewStream(PcapPkt::L4Proto proto)
{
	streams.push_back(Stream3(streamID++, proto));
	return &streams.back();
}

FlowTable<pkt_tuple, uint32_t>::entry* StreamSorter::getFlowEntry(const PcapPkt &pkt)
{
	FlowTable<pkt_tuple, uint32_t>::entry *a;
	struct pkt_tuple pt = pkt.parsePkt();
	Stream3 *stream = NULL;

	a = ft->lookup(pt.flip());
	if (!a) {
		a = ft->lookup(pt);
		if (!a) {
			stream = addNewStream(pkt.getProto());

			a = ft->insert(pt, stream->getID(), pkt.ts());
		}
	}

	if (a->expired(pkt.ts(), streams[a->value].getTimeout())) {
		ft->remove(a);

		stream = addNewStream(pkt.getProto());

		a = ft->insert(pt, stream->getID(), pkt.ts());
	}
	return a;
}

void StreamSorter::processPkt(const PcapPkt &pkt)
{
	FlowTable<pkt_tuple, uint32_t>::entry *a;

	a = getFlowEntry(pkt);
	a->tv = pkt.ts();
	streams[a->value].addPkt(pkt);
}

void StreamSorter::mergeChunks(const string &outputBinFile)
{
	cout << "merging chunks: " << tempFilePath << " to " << outputBinFile << endl;
	cout << "have " << flushOffsets.size() << " parts to merge" << endl;
	MappedFile tempFile;

	if (tempFile.open(tempFilePath)) {
		cerr << "failed to open temp file" << endl;
		return;
	}
	ofstream file;

	file.open(outputBinFile.c_str());

	if (!file.is_open()) {
		cerr << "failed top open file '" << outputBinFile << "'" << endl;
		return;
	}
	MemReader memReader(&tempFile, flushOffsets);
	Stream3 stream;

	Progress progress(memReader.getTotalLength());

	while (memReader.read(&stream)) {
		stream.flush(&file);
		if (progress.couldRefresh()) {
			progress.setProgress(memReader.consumed());
			progress.refresh();
		}
	}

	progress.setProgress();
	progress.refresh(true);
	tempFile.close();
}

void StreamSorter::setTempFileName()
{
	tempFilePath = Path(workingDirectory).add("/tmp").str();
}
