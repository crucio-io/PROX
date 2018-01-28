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

#include <cstdio>
#include <iostream>
#include <iomanip>

#include "timestamp.hpp"

Timestamp Timestamp::operator-(const Timestamp& other) const
{
	uint64_t sec;
	uint64_t nsec;

	if (other.m_nsec <= m_nsec) {
		nsec = m_nsec - other.m_nsec;
		sec = m_sec - other.m_sec;
	} else {
		nsec = (1000000000 + m_nsec) - other.m_nsec;
		sec = m_sec - 1 - other.m_sec;
	}

	return Timestamp(sec, nsec);
}

bool Timestamp::operator>(const Timestamp& other)
{
	return m_sec > other.m_sec ||
		(m_sec == other.m_sec && m_nsec > other.m_nsec);
}

bool Timestamp::operator<(const Timestamp& other)
{
	return m_sec < other.m_sec ||
		(m_sec == other.m_sec && m_nsec < other.m_nsec);
}

ostream& operator<<(ostream& stream, const Timestamp& ts)
{
	stream << ts.m_sec << "." << setw(9) << setfill('0') << ts.m_nsec;
	return stream;
}

double operator/(double d, const Timestamp &denominator)
{
	return d * 1000000000 / (denominator.m_sec * 1000000000 + denominator.m_nsec);
}

bool Timestamp::operator==(const Timestamp &other) const
{
	return m_sec == other.m_sec && m_nsec == other.m_nsec;
}
