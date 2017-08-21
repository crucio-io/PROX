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

#ifndef _CFG_FILE_H_
#define _CFG_FILE_H_

#include <stdio.h>

#define DEFAULT_CONFIG_FILE	"./prox.cfg"

/* configuration file line parser procedure */
typedef int (*cfg_parser)(unsigned sindex, char *str, void *data);

#define CFG_INDEXED	0x80000000	/* section contains index [name #] */
#define MAX_INDEX	64

struct cfg_section {
	const char	*name;	/* section name without [] */
	cfg_parser	parser;	/* section parser function */
	void		*data;	/* data to be passed to the parser */
	/* set by parsing procedure */
	unsigned	indexp[MAX_INDEX];
	int             raw_lines; /* if set, do not remove text after ';' */
	int		nbindex;
	int		error;
};

#define MAX_CFG_STRING_LEN 8192
#define STRING_TERMINATOR_LEN 4

struct cfg_file {
	char		*name;
	FILE		*pfile;
	unsigned	line;
	unsigned	index_line;
	/* set in case of any error */
	unsigned	err_line;
	char		*err_section;
	unsigned	err_entry;
	char		cur_line[MAX_CFG_STRING_LEN + STRING_TERMINATOR_LEN];
};

struct cfg_file *cfg_open(const char *cfg_name);
int cfg_parse(struct cfg_file *pcfg, struct cfg_section *psec);
int cfg_close(struct cfg_file *pcfg);

#endif /* _CFGFILE_H_ */
