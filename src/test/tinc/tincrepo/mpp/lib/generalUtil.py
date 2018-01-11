#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

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
import re
import subprocess
import shutil
import pprint
import time
import logging
from pprint import pformat
from datetime import datetime

class GeneralUtil(object):
    """
    General Utility class
    """

    ###
    def __init__(self):
        """ 
        Constructor 
        """
        self.logger = logging.getLogger()

    ###
    def Now(self):
        """ 
        Current timestamp 
        """
        return time.ctime(time.time())


    ###
    def MakeDirs(self, dirname):
        """ 
        Make directory if not exists 
        @param dirname: directory to be created
        """
        if not os.path.exists(dirname):
            os.makedirs(dirname)

    ###
    def RemoveDirs(self, dirname):
        """ 
        Remove directory if exists 
        @param dirname: directory to be removed
        """
        shutil.rmtree(dirname)

    ###
    def SetupDir(self, dirList):
        """ 
        Remove existing directory and create new one 
        @param dirList: directory list
        """
        for dirname in dirList.split():
            if os.path.exists(dirname):
                self.RemoveDirs(dirname)
            self.MakeDirs(dirname)

    ###
    def GetIfIp(self, ifn):
        """ 
        Get IP address via socket
        @param ifn: network interface
        """ 
        import socket, fcntl, struct
        sck = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        return socket.inet_ntoa(fcntl.ioctl(sck.fileno(), 0x8915, struct.pack('256s', ifn[:15]))[20:24])

    ###
    def GetNetworkInterface(self):
        """ 
        Returns the list of active network interface accept eth0 and lo 
        """
        networkInterface = []
        f = open('/proc/net/dev','r')
        ifacelist = f.read().split('\n')
        f.close()
        ifacelist.pop(0)
        ifacelist.pop(0)
        for line in ifacelist:
            ifacedata = line.replace(' ','').split(':')
            if len(ifacedata) == 2:
                # check the interface is up (Transmit/Receive data)
                if int(ifacedata[1]) > 0:
                    interface = ifacedata[0].strip()
                    if interface != "eth0" and interface != "lo":
                        networkInterface.append(interface)
                        #print self.GetIfIp(ifacedata[0])
        return networkInterface

    ### 
    def FindUsedPort(self, protocol="tcp"):
        """ 
        FindUsedPort returns the list of used ports based on the protocol
        @param protocol: supported protocol are tcp and STREAM
        """
        if protocol == "tcp":
            freePortPat = re.compile('^tcp\s+\d+\s+\d+\s+\S+:+(\d+)')
        elif protocol == "STREAM":
            freePortPat = re.compile('^unix\s+\d+\s+\[.*\]\s+%s\s+\w+\s+(\d+)\s+\S*$' % protocol)
        portCmd = "netstat -an"
        cmdPipe = self.RunCommand(portCmd)
        usedPort = []
        for out in cmdPipe:
            match = freePortPat.match(out)
            if match:
                port = match.group(1)
                usedPort.append(port)
        return usedPort

    ### 
    def RunCommand(self, cmd):
        """ 
        Execute the shell command and retrieve the output 
        @param cmd: command to execute
        """
        cmdPipe = subprocess.Popen(cmd, stdout = subprocess.PIPE, shell=True)
        return cmdPipe.stdout

    ### 
    def ShellCommand(self, cmd, debugFlag=True):
        """ 
        Execute the shell command and retrieve the output 
        @param cmd: command to execute
        """
        self.logger.debug(cmd)
        retdata = []
        timeStart = datetime.now()
        cmdPipe = subprocess.Popen(cmd, stdout = subprocess.PIPE, shell=True)
        while cmdPipe.poll() is None:
            output = cmdPipe.stdout.readline()
            if output:
                retdata.append(output.strip())
        # Wait for the process to finish
        output = cmdPipe.communicate()[0]
        if output:
            retdata.append(output.strip())
        rc = cmdPipe.returncode
        if rc == None:
            self.logger.error("ShellCommand communicate None: rc %s cmd %s", rc, cmd)
        #
        timeEnd = datetime.now()
        duration = timeEnd -  timeStart
        if debugFlag:
            self.logger.debug(pformat(retdata))
        self.logger.debug("rc %s %s", rc, cmd)
        return rc, retdata

    ###
    def GetExistingDb(self):
        """ 
        GetExistingDb retrieve the pre-existing database before test starts 
        """
        rowPat = re.compile('^\((\d+) rows\)')
        psqlStr = "psql template1 -c 'SELECT datname FROM pg_catalog.pg_database'"
        cnt = 0
        preExistingDb = []
        cmdPipe = self.RunCommand(psqlStr)
        for out in cmdPipe:
            cnt += 1
            # Skip the first 2 lines
            if cnt == 1 or cnt == 2:
                continue
            out = out.strip()
            # Break out after matching the row count
            match = rowPat.match(out)
            if match:
                break
            else:
                preExistingDb.append(out)
        return preExistingDb

    ### 
    def DeleteDb(self, dblist):
        """ 
        DeleteDb drops all the database listed in dblist 
        @param dblist: database list
        """
        print 'DeleteDb dblist %s' % (dblist)
        if not dblist:
            return
        # Do not allow removing system database
        systemDbList = ['GreenplumDB', 'postgres', 'template0', 'template1']
        for db in dblist:
            if not db in systemDbList:
                dropdb = "dropdb %s" % (db)
                os.system(dropdb)

