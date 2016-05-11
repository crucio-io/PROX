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

#include <cstdlib>

#include "memreader.hpp"
#include "mappedfile.hpp"
#include "stream3.hpp"

MemReader::MemReader(MappedFile *file, const vector<size_t> &offsets)
{
	initRanges(file->getMapBeg(), file->getMapEnd(), offsets);
}

bool MemReader::read(Stream3 *stream)
{
	if (ranges.empty())
		return false;

	readStream(stream, getLowestID());
	removeEmptyRanges();
	return true;
}

uint32_t MemReader::getLowestID() const
{
	uint32_t lowestID = UINT32_MAX;
	uint32_t rangeID;

	for (size_t i = 0; i < ranges.size(); ++i) {
		rangeID = Stream3::getIDFromMem(ranges[i].first);
		if (rangeID < lowestID)
			lowestID = rangeID;
	}
	return lowestID;
}

void MemReader::readStream(Stream3 *stream, uint32_t id)
{
	stream->removeAllPackets();
	stream->setID(id);
	
	size_t len = 0;
	for (size_t i = 0; i < ranges.size(); ++i) {
		if (Stream3::getIDFromMem(ranges[i].first) == id) {
			stream->addFromMemory(ranges[i].first, &len);
			ranges[i].first += len;
		}
	}
}

void MemReader::removeEmptyRanges()
{
	vector<pair <uint8_t *, uint8_t *> > original = ranges;
	size_t destinationIdx = 0;

	for (size_t i = 0; i < original.size(); ++i) {
		if (original[i].first < original[i].second)
			ranges[destinationIdx++] = original[i];
	}
	ranges.resize(destinationIdx);
}

void MemReader::initRanges(uint8_t *begin, uint8_t *end, const vector<size_t> &offsets)
{
	ranges.resize(offsets.size());

	totalLength = 0;
	for (size_t i = 0; i < offsets.size(); ++i) {
		ranges[i].first = begin + offsets[i];
		if (i != offsets.size() - 1)
			ranges[i].second = begin + offsets[i + 1];
		else
			ranges[i].second = end;
		totalLength += ranges[i].second - ranges[i].first;
	}
	removeEmptyRanges();
}

size_t MemReader::getRangeLengths() const
{
	size_t total = 0;

	for (size_t i = 0; i < ranges.size(); ++i) {
		total += ranges[i].second - ranges[i].first;
	}
	return total;
}

size_t MemReader::consumed() const
{
	return totalLength - getRangeLengths();
}
