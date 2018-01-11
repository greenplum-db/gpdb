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

import time
import platform

from gppylib.commands.base import Command

class GpfdistError(Exception):
    """
    Gpfdist Execption
    @copyright: Pivotal
    """
    pass

class gpfdist:
    """
    GPFDIST helper class to start, stop, check
    
    @copyright: Pivotal 2013
    @note: Command REMOTE does not work properly on Solaris for start and stop gpfdist
    """
    
    def __init__(self, port, hostname):
        """
        gpfdist init
        @var port: Port Number
        @var hostname: Hostname 
        """
        self.port = str(port)
        self.hostname = hostname
        self.secure = False
        self.ssl_cert = ""

        # Ensure we use compatible ps command on Solaris platform
        self.ps_command = 'ps'
        if platform.system() in ['SunOS']:
            self.ps_command = '/bin/ps'

    def start(self, options="", port=None, raise_assert=True, ssl=None):
        """
        start hosting the data
        @var options: gpfdist options
        @var port: Port Number
        @var raise_assert: raise gpfdist error by default
        @var ssl: enable ssl
        @note: previously call cdbfast.GPDFIST.startGpfdist
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

        cmd_str = "gpfdist -p %s %s %s > /dev/null &" % (port, options, ssl)
        cmd = Command("Run gpfdist", cmdStr=cmd_str)
        cmd.run()
        return self.check_gpfdist_process(port=port, raise_assert=raise_assert)

    def check_gpfdist_process(self, wait=60, port=None, raise_assert=True):
        """
        Check for the gpfdist process
        Wait at least 60s until gpfdist starts, else raise an exception
        @var wait: wait at least 60s for gpfdist
        @var port: Port Number
        @var raise_assert: raise gpfdist error by default
        """
        if port is None:
            port = self.port
        process_started = False
        count = 0
        while (not process_started and count<wait):
            cmd_str = "%s -ef | grep \"gpfdist -p %s\" | grep -v grep" % (self.ps_command, port)
            cmd = Command(name='check for gpfdist', cmdStr=cmd_str)
            cmd.run()
            
            content = cmd.get_results().stdout
            if len(content)>0:
                if content.find("gpfdist -p %s" % port)>0:
                    process_started = self.is_gpfdist_connected(port)
                    if process_started:
                        return True
            count = count + 1
            time.sleep(1)
        if raise_assert:
            raise GpfdistError("Could not start gpfdist process")
        else:
            print "Could not start gpfdist process"

    def is_gpfdist_connected(self, port=None):
        """
        Check gpfdist by connecting after starting process
        @var port: Port Number
        @return: True or False
        """
        if port is None:
            port = self.port

        url = "http://%s:%s" % (self.hostname, port)
        if self.secure:
            url =  url.replace("http:", "https:") + " -k"

        cmd_str = "curl %s" % (url)
        cmd = Command(name='check gpfdist is connected', cmdStr=cmd_str)
        cmd.run()

        content = cmd.get_results().stderr
        if content.find("couldn't")>=0 or content.find("Failed to connect")>=0:
            return False
        return True

    def is_port_released(self, port=None):
        """
        Check whether the port is released after stopping gpfdist
        @var port: Port Number
        @return: True or False
        """
        if port is None:
            port = self.port
        cmd_str = "netstat -an |grep '*.%s'" % (port)
        cmd = Command(name='check gpfdist is released', cmdStr=cmd_str)
        cmd.run()
        
        results = cmd.get_results()
        if len(results.stdout)>0 and results.rc==1:
            return False
        return True

    def stop(self, wait=60, port=None):
        """
        kill the gpfdist process
        @var wait: wait at least 60s for gpfdist
        @var port: Port Number
        @note: previously call cdbfast.GPDFIST.killGpfdist
        """
        if port is None:
            port = self.port
        cmd_str = '%s -ef | grep "gpfdist -p %s" | grep -v grep | awk \'{print $2}\' | xargs kill 2>&1 > /dev/null' % (self.ps_command, port)
        cmd = Command(name='stop gpfdist', cmdStr=cmd_str)
        cmd.run()

        is_released = False
        count = 0
        while (not is_released and count<wait):
            is_released = self.is_port_released()
            count = count + 1
            time.sleep(1)
