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

class SystemConfig:
    _user = None
    _ip = None
    _proxDir = None
    _cfgFile = None
    def __init__(self, user, ip, proxDir, configDir):
        self._user = user;
        self._ip = ip;
        self._proxDir = proxDir;
        self._cfgFile = configDir;
    def __init__(self, text):
        self._user = text.split("@")[0];
        text = text.split("@")[1];
        self._ip = text.split(":")[0];
        self._proxDir = text.split(":")[1];
        self._cfgFile = text.split(":")[2];

    def getUser(self):
        return self._user;

    def getIP(self):
        return self._ip;

    def getProxDir(self):
        return self._proxDir;

    def getCfgFile(self):
        return self._cfgFile;

    @staticmethod
    def checkSyntax(text):
        split = text.split("@");
        if (len(split) != 2):
            return SystemConfig.getSyntaxError(text);
        after = split[1].split(":");
        if (len(after) != 3):
            return SystemConfig.getSyntaxError(text);
        return ""
    def toString(self):
        ret = "";
        ret += "  " + self._user + "@" + self._ip + "\n"
        ret += "    " + "prox dir: " + self._proxDir + "\n"
        ret += "    " + "cfg dir: " + self._cfgFile + "\n"
        return ret;

    @staticmethod
    def getSyntaxError(text):
        ret = "Invaild system syntax"
        ret += ", got: " + str(text)
        ret += ", expected: " + str(SystemConfig.expectedSyntax())
        return ret;

    @staticmethod
    def expectedSyntax():
        return "user@ip:proxDir:cfgFile"
