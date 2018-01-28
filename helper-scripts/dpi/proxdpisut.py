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

from prox import *
from remotesystem import *
from time import *
from decimal import *

class ProxDpiSut(Prox):
    def __init__(self, ts, coreCount):
        super(ProxDpiSut, self).__init__(ts)

        self._setDefaultArguments();
        self._setDpiCoreCount(coreCount);

    def _setDefaultArguments(self):
        self.addArgument("-e");
        self.addArgument("-t");
        self.addArgument("-k");
        self.addArgument("-d");
        self.addArgument("-r 0.01");

    def _setDpiCoreCount(self, count):
        self.addArgument("-q dpi_core_count=" + str(count))

    def _querySetup2(self):
        self._query_cores();

    def _query_cores(self):
        print "querying cores"
        self._wk = self._get_core_list("$wk");

    def _get_core_list(self, var):
        ret = []
        result = self._send("echo " + var)._recv();
        for e in result.split(","):
            ret += [e];
        return ret;

    def getTsc(self):
        cmd = "stats task.core(%s).task(0).tsc" % self._wk[-1]
        res = int(self._send(cmd)._recv());
        if (res == 0):
            return self._getTsc();
        else:
            return res;
