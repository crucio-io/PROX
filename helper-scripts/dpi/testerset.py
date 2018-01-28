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

from proxdpitester import *

class testerSet:
    def __init__(self, test_systems, maxRate, testParam):
        self._test_systems = [];
        self._reason = ""
        self._maxRate = maxRate

        testParamPerSystem = testParam.getPerSystem(len(test_systems));

        for i in range(len(test_systems)):
            ts = test_systems[i];
            to_add = ProxDpiTester(ts, testParamPerSystem, i);
            self.add_test_system(to_add);

    def getCount(self):
        return len(self._test_systems);

    def add_test_system(self, test_system):
        self._test_systems.append(test_system);

    def startFork(self):
        print "Starting test systems:"
        for ts in self._test_systems:
            print "\t" + str(ts.getIP())
            ts.startFork();

    def startJoin(self):
        for ts in self._test_systems:
            elapsed = ts.startJoin();
            if (elapsed == None):
                print "Failed to start on " + str(ts.getIP())
            else:
                print "Started on " + str(ts.getIP())
        sleep(1);

    def startForkJoin(self):
        self.startFork();
        self.startJoin();

    def update_stats(self):
        for ts in self._test_systems:
            ts.update_stats();

    def wait_links_up(self):
        for ts in self._test_systems:
            ts.waitAllLinksUp();
        sleep(1);

    def start_cores(self):
        for ts in self._test_systems:
            ts.start_all_ld();
            ts.waitCmdFinished();
        for ts in self._test_systems:
            ts.start_all_workers();
        for ts in self._test_systems:
            ts.waitCmdFinished();

    def stop_cores(self):
        for ts in self._test_systems:
            ts.stop_all_workers();
            ts.stop_all_ld();

        for ts in self._test_systems:
            ts.waitCmdFinished();

    def getTsc(self):
        ret = []
        for ts in self._test_systems:
            ret += [ts.getTsc()]
        return ret;

    def get_setup_rate(self):
        total = 0;
        for ts in self._test_systems:
            total += ts.getCurrentSetupRate();
        return total

    def get_total_connections(self):
        total = 0;
        for ts in self._test_systems:
            ts_tot_conn = ts.get_total_connections();
            total += ts_tot_conn

        return total;

    def get_total_retx(self):
        total = 0;
        for ts in self._test_systems:
            total += ts.get_total_retx();
        return total;

    def getIerrors(self):
        total = 0;
        for ts in self._test_systems:
            total += ts.getIerrorsCached();
        return total;

    def get_rates(self):
        rates = [];
        for ts in self._test_systems:
            rates += ts.get_rates_client_ports();
        return rates;

    def tx_rate_meassurement(self):
        rates = []
        for ts in self._test_systems:
            rates += ts.tx_rate_meassurement();
        return rates;

    def scpStatsDump(self, dst):
        ret = []
        for i in range(len(self._test_systems)):
            dstFileName = dst + str(i);
            ret.append(dstFileName);
            self._test_systems[i].scpStatsDump(dstFileName)
        return ret;

    def conditionsGood(self):
        tot_retx = self.get_total_retx();
        rates = self.get_rates();
        ierrors = self.getIerrors();

        if (tot_retx > 100):
            self._reason = "Too many reTX (" + str(tot_retx) + ")"
            return False;
        if (ierrors > 0):
            self._reason = "Too many ierrors (" + str(ierrors) + ")"
            return False;
        for i in range(0, len(rates)):
            if (rates[i] > self._maxRate):
                self._setReason(i, rates)
                return False;
        return True;

    def _setReason(self, port, rates):
        portStr = str(port);
        rateStr = str(rates[port])
        maxRateStr = str(self._maxRate);
        allRatesStr = str(rates);

        fmt = "Rate on port %s = %s > %s, rate on all = %s"
        self._reason = fmt % (portStr, rateStr, maxRateStr, allRatesStr)

    def getReason(self):
        return self._reason;

    def quitProx(self):
        for ts in self._test_systems:
            ts.quitProx();

    def killProx(self):
        for ts in self._test_systems:
            ts.stop_all_workers();
        for ts in self._test_systems:
            ts.stop_all_ld();
        for ts in self._test_systems:
            ts.killProx();
