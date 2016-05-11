#!/bin/env python
# Copyright(c) 2010-2016 Intel Corporation.
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

from statsconsfile import *
from decimal import *

class SutStatsConsFile:
    def __init__(self, fileName, offset):
        self.offset = offset;
        self.statsConsFile = StatsConsFile(fileName)

    def readNext(self):
        entry = self._readNextEntry();

        if (entry is None):
            return None;

        while (entry is not None and entry[-1] <= 0):
            entry = self._readNextEntry();
        return entry;

    def getHz(self):
        return self.statsConsFile.getHz();

    def _readNextEntry(self):
        entry = self.statsConsFile.readNext();
        if (entry is None):
            return None;

        rx = 0;
        tx = 0;
        drop = 0;
        last_tsc = 0;

        for i in range(0, len(entry), 2):
            rx += entry[i][2]
            tx += entry[i][3]
            drop += entry[i][4]
            last_tsc = entry[i][5]

        last_tsc -= self.offset;
        last_tsc = Decimal(last_tsc) / self.statsConsFile.getHz();
        return [rx, tx, drop, last_tsc];

    def close(self):
        self.statsConsFile.close();
