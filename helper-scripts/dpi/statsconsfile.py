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

import os
import struct

class StatsConsFile:
    def __init__(self, file_name, tsc = None):
        self._file = open(file_name, "rb");
        try:
            data = self._file.read(4*8);
            dataUnpacked = struct.unpack("<qqqq", data);

            self._hz = dataUnpacked[0]
            if (tsc is None):
                self._tsc = dataUnpacked[1]
            else:
                self._tsc = tsc;

            self._entryCount = dataUnpacked[2]
            fieldCount = dataUnpacked[3]

            data = self._file.read(fieldCount);
            fmt = "b" * fieldCount;

            dataUnpacked = struct.unpack("<" + fmt, data);
            self._entryFmt = "<";
            self._entrySize = 0;

            for e in dataUnpacked:
                if (e == 4):
                    self._entryFmt += "i"
                elif (e == 8):
                    self._entryFmt += "q"
                else:
                    raise Exception("Unknown field format: " + str(e))
                self._entrySize += e
        except:
            print "except"
            self._file.close();

    def setBeg(self, tsc):
        self._tsc = tsc

    def getBeg(self):
        return self._tsc;

    def getHz(self):
        return self._hz

    def readNext(self):
        ret = []
        for i in range(self._entryCount):
            entry = self._readNextEntry()
            if (entry == None):
                return None;
            ret.append(entry);
        return ret;

    def _readNextEntry(self):
        try:
            entry = self._file.read(self._entrySize);
            entryUnpacked = struct.unpack(self._entryFmt, entry);
            return list(entryUnpacked)
        except:
            return None;

    def close(self):
        self._file.close();
