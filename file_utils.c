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

#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "prox_args.h"
#include "file_utils.h"

static char file_error_string[128] = {0};

const char *file_get_error(void)
{
	return file_error_string;
}

__attribute__((format(printf, 1 ,2))) static void file_set_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(file_error_string, sizeof(file_error_string), fmt, ap);
	va_end(ap);
}

static void resolve_path_cfg_dir(char *file_name, size_t len, const char *path)
{
	if (path[0] != '/')
		snprintf(file_name, len, "%s/%s", get_cfg_dir(), path);
	else
		strncpy(file_name, path, len);
}

long file_get_size(const char *path)
{
	char file_name[PATH_MAX];
	struct stat s;

	resolve_path_cfg_dir(file_name, sizeof(file_name), path);

	if (stat(file_name, &s)) {
		file_set_error("Stat failed on '%s': %s", path, strerror(errno));
		return -1;
	}

	if ((s.st_mode & S_IFMT) != S_IFREG) {
		snprintf(file_error_string, sizeof(file_error_string), "'%s' is not a file", path);
		return -1;
	}

	return s.st_size;
}

int file_read_content(const char *path, uint8_t *mem, size_t beg, size_t len)
{
	char file_name[PATH_MAX];
	FILE *f;

	resolve_path_cfg_dir(file_name, sizeof(file_name), path);
	f = fopen(file_name, "r");
	if (!f) {
		file_set_error("Failed to read '%s': %s", path, strerror(errno));
		return -1;
	}

	fseek(f, beg, SEEK_SET);

	size_t ret = fread(mem, 1, len, f);
	if ((uint32_t)ret !=  len) {
		file_set_error("Failed to read '%s:%zu' for %zu bytes: got %zu\n", file_name, beg, len, ret);
		return -1;
	}

	fclose(f);
	return 0;
}
