/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2018 Viosoft Corporation.
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

#ifndef _TIMESTAMP_H_
#define _TIMESTAMP_H_

#include <iostream>

#include <sys/time.h>
#include <inttypes.h>

using namespace std;

class Timestamp {
public:
	Timestamp(const uint64_t sec, const uint64_t nsec) : m_sec(sec), m_nsec(nsec) {}
	Timestamp() {}
	Timestamp(const struct timeval& tv) : m_sec(tv.tv_sec), m_nsec(tv.tv_usec) {}
	Timestamp operator-(const Timestamp& other) const;
	bool operator==(const Timestamp &other) const;
	friend double operator/(double d, const Timestamp &denominator);
	bool operator>(const Timestamp& other);
	bool operator<(const Timestamp& other);
	uint64_t sec() const {return m_sec;}
	uint64_t nsec() const {return m_nsec;}
	friend ostream& operator<<(ostream& stream, const Timestamp& ts);
private:
	uint64_t m_sec;
	uint64_t m_nsec;
};

#endif /* _TIMESTAMP_H_ */
