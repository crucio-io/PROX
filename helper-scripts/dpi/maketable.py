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

import sys
from config import *
from csvreader import *
from sets import Set
from csvwriter import *

class ResultEntry:
    def __init__(self):
        self.boundary = None;
        self.cores = {}

    def setBoundary(self, val):
        self.boundary = val;

    def addCoreResult(self, core, val):
        self.cores[core] = val

    def getCoreResult(self, core):
        if (core in self.cores):
            return self.cores[core];
        return None;

    def getBoundary(self):
        return self.boundary;

    def getCores(self):
        return self.cores

    def getMsr(self):
        return self.msr;

class DictEntry:
    def __init__(self, key):
        self.dictionary = {}
        self.entries = []
        self.key = key;

config = Config();
config.parse(sys.argv[0], sys.argv[1:])

err = config.getErrorMakeTable();

if (err is not None):
    print err
    exit(-1);

if (config._debug):
    print "Performance data: " + config.getInputFileName2()
    print "Boundaries: " + config.getInputFileName()

allData = {}

infileFields = []
infileFields += [("msr", "int")]
infileFields += [("conn", "int")]
infileFields += [("ss", "Decimal")]
infileFields += [("bw", "Decimal")]

boundariesFile = CsvReader(infileFields)
boundariesFile.open(config.getInputFileName());
boundaries = boundariesFile.readAll();

cores = Set()

orderedResults = []
finalResults = {}

for a in boundaries:
    key = a["conn"]
    if (key not in finalResults):
        newDict = DictEntry(key)
        finalResults[key] = newDict
        orderedResults.append(newDict)

for a in boundaries:
    table = finalResults[a["conn"]]
    key = a["msr"]
    value = ResultEntry()
    value.msr = a["msr"]
    value.conn = a["conn"]
    value.boundary = a["bw"]
    table.dictionary[key] = value
    table.entries.append(value)

infileFields2 = []
infileFields2 += [("cores", "int")]
infileFields2 += [("msr", "int")]
infileFields2 += [("conn", "int")]
infileFields2 += [("ss", "Decimal")]
infileFields2 += [("down", "Decimal")]

resultsFile = CsvReader(infileFields2)
resultsFile.open(config.getInputFileName2())

for a in resultsFile.readAll():
    table = finalResults[a["conn"]]
    key = a["msr"]
    table.dictionary[key].addCoreResult(a["cores"], a["down"])
    cores.add(a["cores"]);


outputFile = CsvWriter()

outputFile.open(config._output_file_name)

title = ["setup rate", "maximum"]
for e in sorted(cores):
    title += [str(e)]

for a in orderedResults:
    outputFile.write(["connections = " + str(a.key)])
    outputFile.write(title)

    for e in a.entries:
        line = [str(e.getMsr())]
        line += [str(e.getBoundary())]
        for c in sorted(cores):
            if (e.getCoreResult(c) is not None):
                line += [str(e.getCoreResult(c))]
            else:
                line += [""]
        outputFile.write(line)
