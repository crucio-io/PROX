#!/bin/env python

##
# Copyright(c) 2010-2015 Intel Corporation.
# Copyright(c) 2016-2017 Viosoft Corporation.
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

from decimal import *

class CsvReaderError:
    def __init__(self, msg):
        self._msg = msg;

    def __str__(self):
        return self._msg;

class CsvReader:
    def __init__(self, fieldTypes = None):
        self._file_name = None;
        self._fieldTypes = fieldTypes;

    def open(self, file_name):
        self._file = open(file_name, 'r');
        self._file_name = file_name;

    def read(self):
        line = "#"
        while (len(line) != 0 and line[0] == "#"):
            line = self._file.readline();

        if (len(line) != 0):
            return self._lineToEntry(line)
        else:
            return None;

    def _lineToEntry(self, line):
        split = line.strip().split(',');
        if (self._fieldTypes is None):
            return split;
        have = len(split)
        expected = len(self._fieldTypes)
        if (have != expected):
            raise CsvReaderError("Invalid number of fields %d != %d" % (have, expected))

        entry = {};
        for i in range(len(self._fieldTypes)):
            curFieldType = self._fieldTypes[i][1]
            curFieldName = self._fieldTypes[i][0];
            if (curFieldType == "int"):
                entry[curFieldName] = int(split[i])
            elif (curFieldType == "Decimal"):
                entry[curFieldName] = Decimal(split[i])
            else:
                raise CsvReaderError("Invalid field type %s" % curFieldType);
        return entry;

    def readAll(self):
        ret = []
        line = self.read();
        while (line != None):
            ret.append(line);
            line = self.read();
        return ret;

    def close(self):
        self._file.close();
        self._file = None;
