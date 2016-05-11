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

from testerset import *
from proxdpisut import *
from statsconsfile import *
from time import sleep
from time import time
from decimal import *
import copy
from os import system
import socket
from itertools import chain
from math import *
from csvwriter import *
from csvreader import *
from config import *
from progress import *
from resultprocessor import *

def runTest(coreCount, testParam):
    print "Running test with following parameters:"
    print testParam.toString();


    testers = testerSet(config._test_systems, config._max_port_rate, testParam);

    ret = TestResult(testers.getCount());
    thresh = testParam.getConnections() * config._threshold;
    converged = False;

    sut = ProxDpiSut(config._sut, coreCount);

    testers.startFork();
    sut.startFork();
    testers.startJoin();
    sut.startJoin();
    testers.wait_links_up();
    sut.startAllCores();
    sut.waitCmdFinished();
    testers.start_cores();

    ret.addTimeTS(testers.getTsc());
    ret.addTimeSUT(sut.getTsc());

    print "Running until convergence (%s connections)" % str(thresh)
    p = Progress(thresh, ["connections", "setup rate", "reTX"], False);
    while (not converged):
        sleep(config._interCheckDuration)
        testers.update_stats();

        tot = testers.get_total_connections();
        tot_retx = testers.get_total_retx();
        rates = testers.get_rates();
        cur_setup_rate = testers.get_setup_rate();
        ierrors = testers.getIerrors();
        converged = tot >= thresh;

        if (not converged and not testers.conditionsGood()):
            print "conditions are bad: " + testers.getReason();
            sut.forceQuit();
            sut.killProx();
            testers.killProx();
            return None;

        if (sut.getIerrors() != 0):
            testers.killProx();
            print "Sending quit"
            try:
                sut.forceQuit();
            except:
                print "Sending quit failed"
            sut.killProx();
            return None;

        if (config._debug):
            p.setProgress(tot, [tot, cur_setup_rate, tot_retx]);
            print p.toString();

    skipTime = config._skipTime
    print "Connection threshold reached, waiting for " + str(skipTime) + "s, conditions checked = " + str(config._checkConditions)
    while (skipTime > 0):
        skipTime -= config._interCheckDuration
        sleep(config._interCheckDuration)
        testers.update_stats();
        if (config._checkConditions and not testers.conditionsGood()):
            print "conditions are bad: " + testers.getReason();
            sut.forceQuit();
            sut.killProx();
            testers.killProx();
            return False, [];

    ret.addTimeTS(testers.getTsc());
    ret.addTimeSUT(sut.getTsc());

    testers.tx_rate_meassurement();

    testLength = config._testLength
    print "Waiting final " + str(testLength) + "s"
    while (testLength > 0):
        testLength -= config._interCheckDuration
        testers.update_stats();
        if (not testers.conditionsGood()):
            print "conditions are bad: " + testers.getReason();
            sut.forceQuit();
            sut.killProx();
            testers.killProx();
            return None;

        if (sut.getIerrors() != 0):
            testers.killProx();
            print "Sending quit"
            try:
                sut.forceQuit();
            except:
                print "Sending quit failed"
            sut.killProx();
            return None;

        sleep(config._interCheckDuration)

    rates = testers.tx_rate_meassurement();
    ret.addTimeTS(testers.getTsc());
    ret.addTimeSUT(sut.getTsc());

    print "Quiting Prox on SUT"
    # make sure stats are flushed
    sut.quitProx();
    print "Quiting Prox on test system(s)"
    testers.quitProx()

    ret.rates = rates

    sutStatsDump = "stats_dump_sut"
    tsStatsDumpBaseName = "stats_dump_ts"

    sut.scpStatsDump(sutStatsDump);
    tsStatsDump = testers.scpStatsDump(tsStatsDumpBaseName);

    ret.setTSStatsDump(tsStatsDump);
    ret.setSUTStatsDump(sutStatsDump);
    return ret

def meassurePerf(coreCount, maxSetupRate, total_connections, ss_hi):
    iterationCount = 0;
    accuracy = 10**config._accuracy
    ss_lo = 1
    ss_hi = int(round(ss_hi * accuracy, 0))
    success = True;

    downrate = float(0)
    highest_ss = 0
    iterationOverride = [ss_hi, ss_lo];
    while (ss_lo <= ss_hi):
        if (iterationCount < len(iterationOverride)):
            ss = iterationOverride[iterationCount]
        else:
            ss = (ss_lo + ss_hi)/2;

        testParam = TestParameters(maxSetupRate, total_connections, float(ss)/accuracy);

        result = runTest(coreCount, testParam);

        if (result is None):
            success = False
        else:
            rp = ResultProcessor(result)
            rp.process();
            success = rp.percentHandled() > 0.99999

        print "test result = " + str(success)
        if (success):
            ss_lo = ss + 1;
            highest_ss = max(highest_ss, ss);
            print result.rates
            downrate = sum(result.rates)/len(result.rates)
        else:
            ss_hi = ss - 1;
        iterationCount += 1

    return downrate, float(highest_ss)/accuracy

config = Config();
config.parse(sys.argv[0], sys.argv[1:])

err = config.getErrorTestTwo();
if (err is not None):
    print "Invalid configuration: " + err;
    exit(-1);
else:
    print config.toString()

infileFields = []
infileFields += [("msr", "int")]
infileFields += [("conn", "int")]
infileFields += [("ss", "Decimal")]
infileFields += [("bw", "Decimal")]

infile = CsvReader(infileFields);
infile.open(config.getInputFileName())
inputs = infile.readAll()
infile.close();

summary = CsvWriter();
summary.open(config._output_file_name);

print "Will test up to " + str(config._maxDpiCoreCount) + " DPI cores"

for a in inputs:
    for coreCount in range(1, config._maxDpiCoreCount + 1):
        downrate, ss = meassurePerf(coreCount, a["msr"], a["conn"], a["ss"]);
        summary.write([coreCount, a["msr"], a["conn"], ss, downrate]);

summary.close()
