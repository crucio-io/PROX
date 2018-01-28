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

import os
import time
import socket

def ssh(user, ip, cmd):
    # print cmd;
    ssh_options = ""
    ssh_options += "-o StrictHostKeyChecking=no "
    ssh_options += "-o UserKnownHostsFile=/dev/null "
    ssh_options += "-o LogLevel=quiet "
    running = os.popen("ssh " + ssh_options + " " + user + "@" + ip + " \"" + cmd + "\"");
    ret = {};
    ret['out'] = running.read().strip();
    ret['ret'] = running.close();
    if (ret['ret'] == None):
        ret['ret'] = 0;

    return ret;

def ssh_check_quit(obj, user, ip, cmd):
    ret = ssh(user, ip, cmd);
    if (ret['ret'] != 0):
        obj._err = True;
        obj._err_str = ret['out'];
        exit(-1);

class remoteSystem:
    def __init__(self, user, ip):
        self._ip          = ip;
        self._user        = user;

    def run(self, cmd):
        return ssh(self._user, self._ip, cmd);

    def scp(self, src, dst):
        running = os.popen("scp " + self._user + "@" + self._ip + ":" + src + " " + dst);
        return running.close();

    def getIP(self):
        return self._ip
