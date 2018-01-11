"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import os
import platform
import time

import tinctest

from gppylib.commands.base import Command
from tinctest.lib import run_shell_command

class GPFDISTError(Exception):
    pass

class GPFDIST:

    def __init__(self, port, hostname, directory=None):
        "init"
        self.port = port
        self.hostname = hostname
        self.secure = False
        self.ssl_cert = ""
        if directory is None:
            directory = os.getcwd()
        self.directory = directory
        self.gphome = os.environ.get("GPHOME")

        # Ensure we use compatible ps command on Solaris platform
        self.ps_command = 'ps'
        if platform.system() in ['SunOS']:
            self.ps_command = '/bin/ps'

    def gethost(self):
        return self.hostname

    def getport(self):
        return self.port

    def getdir(self):
        return self.directory

    def startGpfdist(self, options="", port=None, raise_assert=True, ssl=None):
        """
        start hosting the data
        @comment: Why do we need to ssh to a host that is localhost
                  killGpfdist does not support kill process on other host
        @note: If we are to use ssh subprocess, we will go to the home folder,
            let's revisit this with remote command so that it works for starting 
            gpfdist on remote host
        """
        if port is None:
            port = self.port
        else:
            port = str(port)
        if ssl is None:
            ssl = ""
        else:
            self.secure = True
            self.ssl_cert = ssl
            ssl = "--ssl %s" % self.ssl_cert

        directory = self.directory

        gpfdist_cmd = "gpfdist -p %s -d %s %s %s" % (port, directory, options, ssl)
        cmd = "gpssh -h %s 'source %s/greenplum_path.sh; %s > /dev/null &'" % (self.hostname, self.gphome, gpfdist_cmd)
        
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command(cmd, 'gpfdist', res)
        if res['rc'] > 0:
            raise Exception("Failed to start gpfdist on host %s and port %s with non-zero rc" %(self.hostname, port))
        return self.check_gpfdist_process(port=port, raise_assert=raise_assert)

    def check_gpfdist_process(self, wait=60, port=None, raise_assert=True):
        """
        Check for the gpfdist process
        Wait at least 60s until gpfdist starts, else raise an exception
        """
        if port is None:
            port = self.port
        process_started = False
        count = 0
        while (not process_started and count<wait):
            cmd_str = " | ".join([
                       self.ps_command + ' -ef',
                       'grep \"[g]pfdist -p %s\"' % (port)])
            cmd = "gpssh -h %s '%s'" %(self.hostname, cmd_str)
            res = {'rc':0, 'stderr':'', 'stdout':''}
            run_shell_command(cmd, 'gpfdist process check', res)
            content = res['stdout']
            if len(content)>0:
                if content.find("gpfdist -p %s" % port)>0:
                    process_started = self.is_gpfdist_connected(port)
                    if process_started:
                        return True
            count = count + 1
            time.sleep(1)
        if raise_assert:
            raise GPFDISTError("Could not start gpfdist process")
        else:
            tinctest.logger.warning("Could not start gpfdist process")

    def is_gpfdist_connected(self, port=None):
        """
        Check gpfdist by connecting after starting process
        @return: True or False
        @todo: Need the absolute path
        """
        if port is None:
            port = self.port

        url = "http://%s:%s" % (self.hostname, port)
        if self.secure:
            url =  url.replace("http:", "https:") + " -k"

        cmd_str = "curl %s" %url
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command(cmd_str, 'gpfdist process check', res)
        content = res['stdout']
        if content.find("couldn't")>=0:
            return False
        return True

    def is_port_released(self, port=None):
        """
        Check whether the port is released after stopping gpfdist
        @return: True or False
        """
        if port is None:
            port = self.port
        cmd_str = "netstat -an | grep %s" % port
        cmd = "gpssh -h %s '%s'" %(self.hostname, cmd_str)
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command(cmd, 'gpfdist port check', res)
        content = res['stdout']
        # strip hostname prefix from gpssh output
        content = content.replace(self.hostname, '').strip('[]').strip()
        if len(content)>0:
            return False
        return True

    def is_gpfdist_killed(self, port=None, wait=1):
        """
        Check whether the gpfdist process is killed
        """
        if port is None:
            port = self.port
        process_killed = False
        count = 0
        while (not process_killed and count < wait):
            cmd_str = " | ".join([
                       self.ps_command + ' -ef',
                       'grep \"[g]pfdist -p %s\"' % (port)])
            cmd = "gpssh -h %s '%s'" %(self.hostname, cmd_str)
            res = {'rc':0, 'stderr':'', 'stdout':''}
            run_shell_command(cmd, 'gpfdist process check', res)
            content = res['stdout']
            # strip hostname prefix from gpssh output
            content = content.replace(self.hostname, '').strip('[]').strip()
            if len(content)>0 or content.find("gpfdist -p %s" %port) > 0:
                tinctest.logger.warning("gpfdist process still exists on %s:%s" %(self.hostname, self.port))
            else:
                return True
            count = count + 1
            time.sleep(1)
        tinctest.logger.warning("gpfdist process not killed on %s:%s" %(self.hostname, self.port))
        return False

    def killGpfdist(self, wait=60, port=None):
        """
        kill the gpfdist process
        @change: Johnny Soedomo, check from netstat whether the system has released the process rather than waiting a flat 10s
        @todo: Support for stopping gpfdist process on remote host
        """
        if port is None:
            port = self.port

        cmd_str = ' | '.join([self.ps_command + " -ef",
                              "grep \"[g]pfdist -p %s\"" % (port),
                              "awk '\"'\"'{print $2}'\"'\"'",
                              "xargs kill"])
        cmd = "gpssh -h %s '%s'" %(self.hostname, cmd_str)

        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command(cmd, 'kill gpfdist', res)

        if not self.is_gpfdist_killed():
            raise GPFDISTError("Could not kill gpfdist process on %s:%s" %(self.hostname, self.port))
        # Make sure the port is released
        is_released = False
        count = 0
        while (not is_released and count < wait):
            is_released = self.is_port_released()
            count = count + 1
            time.sleep(1)
