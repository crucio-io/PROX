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

#ifndef _PROGRESS_H_
#define _PROGRESS_H_

#include <inttypes.h>
#include <vector>
#include <utility>
#include <string>

using namespace std;

class Progress {
public:
	Progress(size_t maxProgress, bool inPlace = true, bool showElapsedTime = true);
	void setTitle(const string &prefix) {this->title = title;}
	void setProgress(size_t curProgress);
	void setProgress();
	uint32_t addDetail(const string& detail);
	void clearDetails() {details.clear();}
	void setDetail(uint32_t idx, uint32_t val);
	bool couldRefresh();
	void refresh(bool withNewLine = false);
private:
	uint64_t firstRefresh;
	uint64_t lastRefresh;
	size_t maxProgress;
	size_t curProgress;
	bool inPlace;
	bool showElapsedTime;
	size_t prevLength;
	string title;
	vector<pair<string, uint32_t> > details;
};

#endif /* _PROGRESS_H_ */
