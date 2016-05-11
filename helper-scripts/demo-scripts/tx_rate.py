#!/bin/env python2.7
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

from prox import *
from decimal import *
from time import *

class data_point:
    value = 0;
    tsc = 0;
    def __init__(self, value, tsc):
        self.value = value;
        self.tsc = tsc;

def measure_tx(prox_instance, port_id):
    port_tx_pkt = "port(" + str(port_id) + ").tx.packets"
    port_tsc = "port(" + str(port_id) + ").tsc";
    cmd = "stats " + port_tx_pkt + "," + port_tsc;
    reply = prox_instance.send(cmd).recv().split(",");

    return data_point(int(reply[0]), int(reply[1]));

def get_rate(first, second, hz):
    tsc_diff = second.tsc - first.tsc;
    value_diff = second.value - first.value;

    return int(Decimal(value_diff * hz) / tsc_diff)

# make sure that prox has been started with the -t parameter
prox_instance = prox("127.0.0.1")
print "Connected to prox"

hz = int(prox_instance.send("stats hz").recv());

print "System is running at " + str(hz) + " Hz"

print "Showing TX pps on port 0"

update_interval = 0.1

print "Requesting new data every " + str(update_interval) + "s"

measure = measure_tx(prox_instance, 0);
while (True):
    sleep(update_interval)
    measure2 = measure_tx(prox_instance, 0);

    # since PROX takes measurements at a configured rate (through
    # update interval command or throw -r command line parameter), it
    # might be possible that two consecutive measurements report the
    # same. To get updates at a frequency higher than 1 Hz,
    # reconfigure prox as mentioned above.

    if (measure.tsc == measure2.tsc):
        continue;

    print get_rate(measure, measure2, hz);

    measure = measure2;
