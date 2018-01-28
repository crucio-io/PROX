#!/bin/bash

##
# Copyright(c) 2010-2015 Intel Corporation.
# Copyright(c) 2016-2018 Viosoft Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

bad_lines=$(grep -nHr -e "[[:space:]]$" *.c *.h gen/*.cfg config/*.cfg)

if [ -n "$bad_lines" ]; then
    echo "Found trailing white-spaces:"
    echo $bad_lines
    exit 1;
fi

for f in *.c *.h gen/*.cfg config/*.cfg; do
    result=$(tail -n 1 $f | grep "^$" | wc -l)

    if [ "$result" == "1" ]; then
        echo "Trailing newlines at end of file $f"
        exit 1
    fi
done;

prev="dummy"
function findDuplicate() {
    line=1
    while read p; do
	if [ "$prev" == "" ]; then
	    if [ "$p" == "" ]; then
		echo "duplicate empty line at $1:$line"
		bad=1
	    fi
	fi
	prev=$p
	let "line+=1"
    done <$1
}

bad=0
for f in *.c *.h; do
    findDuplicate $f
done;

if [ "$bad" != "0" ]; then
    exit 1
fi

tab="	"
bad_lines=$(grep -nHr -e "^$tab$tab$tab$tab$tab$tab$tab" *.c *.h | head -n1)

if [ -n "$bad_lines" ]; then
    echo "Code nested too deep:"
    echo $bad_lines
    exit 1;
fi

exit 0
