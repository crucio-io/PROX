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

#include <sys/time.h>
#include <iostream>
#include <cstdio>
#include <sstream>

#include "progress.hpp"

static uint64_t getSec()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec;
}

Progress::Progress(size_t maxProgress, bool inPlace, bool showElapsedTime)
	: maxProgress(maxProgress), curProgress(0), inPlace(inPlace), showElapsedTime(showElapsedTime), prevLength(0), title("Progress")
{
	lastRefresh = -1;
	firstRefresh = getSec();
}

void Progress::setProgress(size_t curProgress)
{
	this->curProgress = curProgress;
}

void Progress::setProgress()
{
	this->curProgress = maxProgress;
}

uint32_t Progress::addDetail(const string& detail)
{
	details.push_back(make_pair(detail, 0));
	return details.size() - 1;
}

void Progress::setDetail(uint32_t idx, uint32_t val)
{
	details[idx].second = val;
}

bool Progress::couldRefresh()
{
	uint32_t cur = getSec();

	return (lastRefresh != cur);
}

void Progress::refresh(bool withNewLine)
{
	lastRefresh = getSec();
	uint64_t elapsed = lastRefresh - firstRefresh;
	size_t progress = curProgress * 100 / maxProgress;
	size_t remainingTime = curProgress? (elapsed * maxProgress - elapsed * curProgress) / curProgress : 0;

	stringstream ss;

	if (inPlace)
		ss << "\r";
	ss << title << ": " << progress << "%";
	ss << ", remaining: " << remainingTime;
	if (showElapsedTime)
		ss << ", elapsed: " << elapsed;
	for (size_t i = 0; i < details.size(); ++i)
		ss << ", " << details[i].first << ": " << details[i].second;

	size_t prevLength2 = ss.str().size();

	while (ss.str().size() < prevLength)
		ss << " ";
	prevLength = prevLength2;

	if (!inPlace || withNewLine)
		ss << "\n";

	cout << ss.str();
	cout.flush();
}
