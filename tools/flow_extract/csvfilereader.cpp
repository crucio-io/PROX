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

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdint.h>

#include "csvfilereader.hpp"

int CsvFileReader::open(const string& str)
{
	char *resolved_path = new char[1024];

	memset(resolved_path, 0, 1024);
	realpath(str.c_str(), resolved_path);
	file.open(resolved_path);

	delete []resolved_path;
	return file.is_open();
}

vector<string> CsvFileReader::read()
{
	vector<string> ret;
	size_t prev = 0, cur = 0;
	string line;

	if (file.eof())
		return vector<string>();

   	std::getline(file, line);
	if (line.empty())
		return vector<string>();

	while (true) {
		cur = line.find_first_of(',', prev);

		if (cur != SIZE_MAX) {
			ret.push_back(line.substr(prev, cur - prev));
			prev = cur + 1;
		}
		else {
			ret.push_back(line.substr(prev, line.size() - prev));
			break;
		}
	}
	return ret;
}

void CsvFileReader::close()
{
	file.close();
}
