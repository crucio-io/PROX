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

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <cerrno>
#include <sys/mman.h>
#include <cstring>

#include "mappedfile.hpp"

static void zeroOutFile(int fd, size_t size)
{
	void *empty = calloc(1, 4096);

	while (size > 4096) {
		write(fd, empty, 4096);
		size -= 4096;
	}
	write(fd, empty, size);
	free(empty);
}

int MappedFile::open(const string& filePath, size_t size)
{
	mappedFileSize = size;

	fd = ::open(filePath.c_str(), O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		cerr << "Failed to open file " << filePath << ":" << strerror(errno) << endl;
		return -1;
	}

	zeroOutFile(fd, size);
	data = mmap(NULL, mappedFileSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

	if (data == MAP_FAILED) {
		cerr << "Failed to map file: " << strerror(errno) << endl;
		return -1;
	}
	return 0;
}

static size_t getFileSize(const string& filePath)
{
	struct stat s;
	if (stat(filePath.c_str(), &s))
		return -1;

	return s.st_size;
}

int MappedFile::open(const string& filePath)
{
	mappedFileSize = getFileSize(filePath);

	fd = ::open(filePath.c_str(), O_RDONLY);
	if (fd < 0) {
		cerr << "Failed to open file " << filePath << ":" << strerror(errno) << endl;
		return -1;
	}

	data = mmap(NULL, mappedFileSize, PROT_READ, MAP_SHARED, fd, 0);

	if (data == MAP_FAILED) {
		cerr << "Failed to map file: " << strerror(errno) << endl;
		return -1;
	}
	return 0;
}

int MappedFile::sync()
{
	if (msync(data, mappedFileSize, MS_SYNC) == -1) {
		cerr << "Failed to sync: " << strerror(errno) << endl;
		return -1;
	}
	return 0;
}


void MappedFile::close()
{
	sync();
	munmap(data, mappedFileSize);
	::close(fd);
}

size_t MappedFile::size() const
{
	return mappedFileSize;
}
