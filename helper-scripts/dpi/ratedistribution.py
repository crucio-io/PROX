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
from decimal import *

def usage(progName):
    print "usage: " + progName + " config [up|down]"
    print " The script reads a lua configuration "
    print " and outputs a histogram wit 21 buckets."
    print " The first 20 buckets contain 70th percentile."
    print " The last bucket contains the remaining items."
    exit(-1);

if (len(sys.argv) != 3):
    usage(sys.argv[0])

if (sys.argv[2] == "down"):
    match = "dn_bps"
elif (sys.argv[2] == "up"):
    match = "up_bps"
else:
    usage(sys.argv[0])

values = []
for line in open(sys.argv[1]).readlines():
    line = line.strip();

    if line.find(match) != -1:
        v = line.split(" = ")[1].strip(",")
        values.append(Decimal(v));

values = sorted(values)

treshold = values[int(len(values)*0.7)]

buckets = [0]*21;

for v in values:
    if (v > treshold):
        buckets[20] += 1
    else:
        buckets[int(v * 20 / treshold)] += 1

stepSize = treshold / 20;

print "# bucket range, count"
for i in range(len(buckets) - 1):
    beg = str(int(i * stepSize))
    end = str(int((i + 1) * stepSize - 1))
    print beg + "-" + end + "," + str(buckets[i])

i = len(buckets) - 1
print beg + "+," + str(buckets[i])
