#!/bin/env python

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

from decimal import *
from time import time

class Progress:
    def __init__(self, limit, fieldNames = [], overallETA = True):
        self._fieldNames = fieldNames;
        self._limit = limit;
        self._progress = 0;
        self._prevProgress = 0;
        self._prevTime = 0;
        self._progressSetCount = 0;
        self._time = 0;
        self._overallETA = overallETA;

    def setProgress(self, progress, fieldValues = []):
        self._fieldValues = fieldValues;
        if (self._overallETA == True):
            self._progress = progress
            self._time = time();
            if (self._progressSetCount == 0):
                self._prevProgress = self._progress;
                self._prevTime = self._time;
        else:
            self._prevProgress = self._progress;
            self._prevTime = self._time;
            self._progress = progress;
            self._time = time();
        self._progressSetCount += 1

    def incrProgress(self):
        self.setProgress(self._progress + 1);

    def toString(self):
        ret = ""
        ret += str(self._getETA()) + " seconds left"
        for f,v in zip(self._fieldNames, self._fieldValues):
            ret += ", %s=%s" % (str(f),str(v))
        return ret;

    def _getETA(self):
        if (self._progressSetCount < 2):
            return "N/A"
        diff = self._progress - self._prevProgress;
        t_diff = Decimal(self._time - self._prevTime);
        if (t_diff < 0.001 or diff <= 0):
            return "N/A"
        rate = Decimal(diff)/t_diff
        remaining = Decimal(self._limit - self._progress);
        return round(remaining/rate, 2);
