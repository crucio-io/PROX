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

from __future__ import print_function

import os
import subprocess
import socket

class prox_ctrl(object):
    def __init__(self, ip, key=None, user=None):
        self._ip   = ip
        self._key  = key
        self._user = user
        self._children = []
        self._proxsock = []

    def ip(self):
        return self._ip

    def connect(self):
        """Simply try to run 'true' over ssh on remote system.
        On failure, raise RuntimeWarning exception when possibly worth
        retrying, and raise RuntimeError exception otherwise.
        """
        return self.run_cmd('true', True)

    def close(self):
        """Must be called before program termination."""
        for prox in self._proxsock:
            prox.quit()
        children = len(self._children)
        if children == 0:
            return
        if children > 1:
            print('Waiting for %d child processes to complete ...' % children)
        for child in self._children:
            ret = os.waitpid(child[0], os.WNOHANG)
            if ret[0] == 0:
                print("Waiting for child process '%s' to complete ..." % child[1])
                ret = os.waitpid(child[0], 0)
            rc = ret[1]
            if os.WIFEXITED(rc):
                if os.WEXITSTATUS(rc) == 0:
                    print("Child process '%s' completed successfully" % child[1])
                else:
                    print("Child process '%s' returned exit status %d" % (
                            child[1], os.WEXITSTATUS(rc)))
            elif os.WIFSIGNALED(rc):
                print("Child process '%s' exited on signal %d" % (
                        child[1], os.WTERMSIG(rc)))
            else:
                print("Wait status for child process '%s' is 0x%04x" % (
                        child[1], rc))

    def run_cmd(self, command, _connect=False):
        """Execute command over ssh on remote system.
        Wait for remote command completion.
        Return command output (combined stdout and stderr).
        _connect argument is reserved for connect() method.
        """
        cmd = self._build_ssh(command)
        try:
            return subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as ex:
            if _connect and ex.returncode == 255:
                raise RuntimeWarning(ex.output.strip())
            raise RuntimeError('ssh returned exit status %d:\n%s'
                    % (ex.returncode, ex.output.strip()))

    def fork_cmd(self, command, name=None):
        """Execute command over ssh on remote system, in a child process.
        Do not wait for remote command completion.
        Return child process id.
        """
        if name is None:
            name = command
        cmd = self._build_ssh(command)
        pid = os.fork()
        if (pid != 0):
            # In the parent process
            self._children.append((pid, name))
            return pid
        # In the child process: use os._exit to terminate
        try:
            # Actually ignore output on success, but capture stderr on failure
            subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as ex:
            raise RuntimeError("Child process '%s' failed:\n"
                    'ssh returned exit status %d:\n%s'
                    % (name, ex.returncode, ex.output.strip()))
        os._exit(0)

    def prox_sock(self, port=8474):
        """Connect to the PROX instance on remote system.
        Return a prox_sock object on success, None on failure.
        """
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect((self._ip, port))
            prox = prox_sock(sock)
            self._proxsock.append(prox)
            return prox
        except:
            return None

    def scp_put(self, src, dst):
        """Copy src file from local system to dst on remote system."""
        cmd = [ 'scp',
                '-B',
                '-oStrictHostKeyChecking=no',
                '-oUserKnownHostsFile=/dev/null',
                '-oLogLevel=ERROR' ]
        if self._key is not None:
            cmd.extend(['-i', self._key])
        cmd.append(src)
        remote = ''
        if self._user is not None:
            remote += self._user + '@'
        remote += self._ip + ':' + dst
        cmd.append(remote)
        try:
            # Actually ignore output on success, but capture stderr on failure
            subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as ex:
            raise RuntimeError('scp returned exit status %d:\n%s'
                    % (ex.returncode, ex.output.strip()))

    def _build_ssh(self, command):
        cmd = [ 'ssh',
                '-oBatchMode=yes',
                '-oStrictHostKeyChecking=no',
                '-oUserKnownHostsFile=/dev/null',
                '-oLogLevel=ERROR' ]
        if self._key is not None:
            cmd.extend(['-i', self._key])
        remote = ''
        if self._user is not None:
            remote += self._user + '@'
        remote += self._ip
        cmd.append(remote)
        cmd.append(command)
        return cmd

class prox_sock(object):
    def __init__(self, sock):
        self._sock = sock
        self._rcvd = b''

    def quit(self):
        if self._sock is not None:
            self._send('quit')
            self._sock.close()
            self._sock = None

    def start(self, cores):
        self._send('start %s' % ','.join(map(str, cores)))

    def stop(self, cores):
        self._send('stop %s' % ','.join(map(str, cores)))

    def speed(self, speed, cores, tasks=None):
        if tasks is None:
            tasks = [ 0 ] * len(cores)
        elif len(tasks) != len(cores):
            raise ValueError('cores and tasks must have the same len')
        for (core, task) in zip(cores, tasks):
            self._send('speed %s %s %s' % (core, task, speed))

    def reset_stats(self):
        self._send('reset stats')

    def core_stats(self, cores, task=0):
        rx = tx = drop = tsc = hz = 0
        self._send('core stats %s %s' % (','.join(map(str, cores)), task))
        for core in cores:
            stats = self._recv().split(',')
            rx += int(stats[0])
            tx += int(stats[1])
            drop += int(stats[2])
            tsc = int(stats[3])
            hz = int(stats[4])
        return rx, tx, drop, tsc, hz

    def set_random(self, cores, task, offset, mask, length):
        self._send('set random %s %s %s %s %s' % (','.join(map(str, cores)), task, offset, mask, length))

    def set_size(self, cores, task, pkt_size):
        self._send('pkt_size %s %s %s' % (','.join(map(str, cores)), task, pkt_size))

    def set_value(self, cores, task, offset, value, length):
        self._send('set value %s %s %s %s %s' % (','.join(map(str, cores)), task, offset, value, length))

    def _send(self, cmd):
        """Append LF and send command to the PROX instance."""
        if self._sock is None:
            raise RuntimeError("PROX socket closed, cannot send '%s'" % cmd)
        self._sock.sendall(cmd.encode() + b'\n')

    def _recv(self):
        """Receive response from PROX instance, and return it with LF removed."""
        if self._sock is None:
            raise RuntimeError("PROX socket closed, cannot receive anymore")
        pos = self._rcvd.find(b'\n')
        while pos == -1:
            self._rcvd += self._sock.recv(256)
            pos = self._rcvd.find(b'\n')
        rsp = self._rcvd[:pos]
        self._rcvd = self._rcvd[pos+1:]
        return rsp.decode()

