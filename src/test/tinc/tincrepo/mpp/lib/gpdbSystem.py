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

Capture the information regarding gpdb
"""

import os
import sys
import re
import random
import subprocess
import time
import logging
from optparse import OptionParser

from mpp.lib.gpdbSegmentConfig import GpdbSegmentConfig
#from gpdbFileSpace import GpdbFileSpace
#from Shell import Shell
import tinctest
from mpp.lib.generalUtil import GeneralUtil
from mpp.lib.mppUtil import hasExpectedStr
from mpp.lib.PSQL import PSQL

#####
class GpdbSystem:
    """
    Capture the information regarding gpdb
    @class GpdbSystem
    
    @organization: DCD Partner Engineering
    @contact: Kenneth Wong
    """

    ###
    def __init__(self):
        """
        Constructor for GpdbSystem
        """
        self.logger = logging.getLogger()
        #self.shell = Shell()
        self.generalUtil = GeneralUtil()
        self.errorCnt = {}
        self.errorCheckList = ['gpdbIsUp', 'gpdbIsDown', 'setGpdbUpErrorCnt']
        for key in self.errorCheckList:
            self.errorCnt[key] = 0
        # Capture the gpdb configuration information 
        self.SetClusterConfig()
        # Pattern matchin for starting database
        self.gpstartPattern = re.compile('^\d{8}\:\d\d:\d\d\:\d\d\:gpstart\:(\S+)\:(\w+)\-\[INFO\]\:\-Database successfully started with no errors reported$')
        self.gpstateDownPattern = re.compile('^\d{8}\:\d\d:\d\d\:\d\d\:gpstate\:(\S+)\:(\w+)\-\[CRITICAL\]\:\-gpstate failed.')
        self.gpstartNoArgNoPattern = re.compile('^\> \d{8}\:\d\d:\d\d\:\d\d\:gpstart\:(\S+)\:(\w+)\-\[INFO\]\:\-User abort requested, Exiting...$')
        self.gpstartNoScreenOutputPattern = re.compile('^Continue with Greenplum instance startup Yy\|Nn \(default\=N\)\:')
        # Pattern matchin for stopping database
        self.gpstopPattern = re.compile('^\d{8}\:\d\d:\d\d\:\d\d\:gpstop\:(\S+)\:(\w+)\-\[INFO\]\:\-Database successfully shutdown with no errors reported$')
        self.is_debug = False

    ###
    def SetGpdbUp(self, cmd="gpstart -a"):
        """
        Start Gpdb is not up
        @param cmd: Command to start GPDB
        """
        rc = 0
        rc, out = self.generalUtil.ShellCommand(cmd)
        self.logger.debug("SetGpdbUp rc %s", rc)
        return rc
            
    ###
    def SetGpdbDown(self, cmd="gpstop -a -M immediate"):
        """
        Start Gpdb is not up
        @param cmd: Command to start GPDB
        """
        rc = 0
        if self.SetGpdbUp(cmd):
            rc, out = self.generalUtil.ShellCommand(cmd)
            self.logger.debug("SetGpdbDown rc %s", rc)
        else:
            self.logger.debug("SetGpdbDown GPDB already down")
        return rc
            
    ###
    def GpdbIsUp(self, cmd):
        """
        Return True if GPDB is up else return false using pg_controldata
        @param cmd: Cmd to check whether gpdb is up or down
        """
        gpdbStatePat = re.compile(r'^Database cluster state:\s+(\w+)')
        rc, out = self.generalUtil.ShellCommand(cmd)
        for line in out:
            line = line.strip()
            match = gpdbStatePat.match(line)
            if match:
                gpdbState = match.group(1)
                if gpdbState == 'shut':
                    self.logger.debug("GPDB is down")
                    return False
                else:
                    self.logger.debug("GPDB is up")
                    return True

    ###
    def __str__(self):
        """
        Returns the cluster info as a dictionary
        @return: cluster info
        """
        #return '%s' % (self.cluster)
        return ""

    ###
    def GetGpdbVersion(self):
        """
        
        @summary: Returns the version and build number of the Greenplum Database
        @return: (version, build)
        """

        sql_cmd = 'select version();'
        cmd = PSQL(sql_cmd = sql_cmd, flags = '-q -t', dbname=os.environ.get('PGDATABASE'))
        tinctest.logger.info("Running command - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        ok = not result.rc
        out = result.stdout.strip()

        assert ok, 'Running select version(); returned non-zero code.'
        assert len(out) > 0, 'select version() did not return any rows'
        
        # Assumption is that version is enclosed in parenthesis: (Greenplum Database 4.2.1.0 build 1)
        version_st = out.find('(') + 1
        version_end = out.find(')')
        version_str = out[version_st:version_end]
        
        gpdb_version_prefix = 'Greenplum Database '
        build_prefix = 'build '

        assert version_str.find(gpdb_version_prefix) == 0, "'%s' not found in output of select version();" % gpdb_version_prefix
        assert version_str.find(build_prefix) > (version_str.find(gpdb_version_prefix) + len(gpdb_version_prefix)), "'%s' not found after '%s' in output of select version();" % (build_prefix, gpdb_version_prefix)

        build_st = version_str.find('build')
        build_end = build_st + len(build_prefix)
        
        # version is in between 'Greenplum Database ' and 'build ': Greenplum Database 4.2.1.0 build 1
        version = version_str[len(gpdb_version_prefix):build_st].strip()
        # build number is after 'build ': Greenplum Database 4.2.1.0 build 1
        build = version_str[build_end:].strip()

        # If "(with assert checking)" is presented, then it is a debug build 
        # Added by Hai Huang
        if (hasExpectedStr(out, '(with assert checking)')):
            version += "_debug"
            self.is_debug = True
            
        # Strips the STRING portion from the version string
        if re.match(".*_.+$", version) and version.find("_debug") == -1:
            version = re.sub(r'_.+$', r'', version)

        return (version, build)        

    def is_debug_build(self):
        """
        Check GPDB version whether it is a debug build
        """
        return self.is_debug


    def is_rc_build(self, version):
        """
        Check GPDB version whether it's RC build
        @version: 3.3.3.3, 4.4.4.4, 4.1.1.1a, 4.3_PARISTX, 4.2_private_build, private_build, etc
        """
        if os.environ.get("BUILD_TYPE", "None").lower() == "rc" or re.match("\d+.\d+.\d+.\d+[azAZ]*", version):
            return True
        return False
    
    def GetParsedGpdbVersion(self):
        """
        
        @summary: Parses out GPDB version string into a list (e.g. 4.2.1.0 -> [4,2,1,0]) for processing in tests
        @return: list containing strings for each version number in version string (e.g. 4.2.1.0 -> [4,2,1,0])
        """
        (version, build) = self.GetGpdbVersion()
        parsed_version = re.split('\.|_|-', version)
        return parsed_version
    
    def GetGpInfo(self, label=None):
        """ 

        """
        if label:
            print "GetGpInfo %s entered" % label
        self.generalUtil.ShellCommand("env | grep MASTER_DATA_DIRECTORY")
        self.generalUtil.ShellCommand("env | grep GPHOME")
        self.generalUtil.ShellCommand("env | grep PGPORT")
        cmd = "psql gptest -c 'SELECT VERSION()'"
        self.generalUtil.ShellCommand("gpstate -v -s")
        self.generalUtil.ShellCommand("psql gptest -c 'SELECT * FROM gp_segment_configuration'")
        self.generalUtil.ShellCommand("ps -ef | grep postgres | grep -v grep")
        self.generalUtil.ShellCommand("df -k")
        self.generalUtil.ShellCommand("free")
        self.generalUtil.ShellCommand("vmstat 5 5")
        os.system("ps -ef|grep TEST.py|grep -v grep|awk '{print $2}'|xargs -I '{}' pstree -l -a '{}'")
        '''
        #rc, out = self.generalUtil.ShellCommand("ps -ef|grep TEST.py|grep -v grep|awk '{print $2}'|xargs -I '{}' pstree -l -a -G '{}'")
        rc, out = self.generalUtil.ShellCommand("ps -ef | grep TEST.py | grep -v grep | awk '{print $2}'")
        if out:
            pid = out[0]
            pid = pid.strip()
            cmd = "../lib/getpstree.sh %s" % (pid)
            self.generalUtil.ShellCommand(cmd)
        '''
        if label:
            print "GetGpInfo %s exit" % label

    ###
    def SetClusterConfig(self):
        """
        Query the system to set the cluster configuration information and store in object
        @return: None
        """
        try:
            self.gpdbSegmentConfig = GpdbSegmentConfig()
            #self.gpdbFileSpace = GpdbFileSpace()
            cluster = {}
            cluster['gphome'] = os.environ['GPHOME']
            cluster['pgport'] = os.environ['PGPORT']
            cluster['datadir'] = self.gpdbSegmentConfig.GetMasterDataDirectory()
            rc, cluster['masterHost'] = self.gpdbSegmentConfig.GetMasterHost()
            rc, cluster['countSegments'] = self.gpdbSegmentConfig.GetSegmentServerCount()
            rc, cluster['masterStandbyHost'] = self.gpdbSegmentConfig.GetMasterStandbyHost()
            segmentHosts = {}
            # Set the host and port number for the segment
            for segNum in range(int(cluster['countSegments'])):
                (rc, host, port) = self.gpdbSegmentConfig.GetHostAndPort(segNum)
                segmentHosts[segNum] = host + '_' + port
            cluster['segmentHosts'] = segmentHosts
            self.cluster = cluster
        except KeyError:
            print sys.exc_type, ":", "%s is not in the list." % (sys.exc_value)
            sys.exit(1)


    ###
    ### 
    ###

    #
    # Check the command output against the specified pattern
    #
    ###
    def RunCommand(self, cmd, cmdPattern):
        found = False
        cmdPipe = self.ExecuteCommand(cmd)
        for line in cmdPipe:
             data = line.strip()
             match = cmdPattern.match(data)
             if match:
                 found = True
        #if found == False:
        #     print 'RunCommand: %s found = %s' % (cmd, found)
        return found

    #
    # Execute the command and return the output in stdout
    #
    def ExecuteCommand(self, cmd):
        cmdPipe = subprocess.Popen(cmd, stdout = subprocess.PIPE, shell=True)
        return cmdPipe.stdout

    #
    # Return the current timestamp
    #
    def Now(self):
        return time.ctime(time.time())

    #
    # Start gpdb and return true if successful
    #
    def StartGpdb(self):
        return self.RunCommand("gpstart -a", self.gpstartPattern)

    #
    # Stop gpdb and return true if successful
    #
    def StopGpdb(self):
        return self.RunCommand("gpstop -a -M fast", self.gpstopPattern)

    ###
    def StopGpdbImmediate(self):
        """ Stop the gpdb database immediately """
        return self.RunCommand("gpstop -a -M immediate", self.gpstopPattern)

    #
    # Return true if gpdb is down
    #
    def GpdbDown(self):
        return self.RunCommand("gpstate -s", self.gpstateDownPattern)

    '''
    #
    # Verify that gpdb is up and log any error
    #
    def GpdbIsUp(self, comment=""):
        if self.GpdbDown() == False:
            print 'gpdb is up. ', comment
            return True
        else:
            print 'gpdb is not up. ', comment
            self.errorCnt['gpdbIsUp'] = 1 + self.errorCnt.get('gpdbIsUp')
            return False

    #
    # Verify that gpdb is down and log any error
    #
    def GpdbIsDown(self, comment=""):
        if self.GpdbDown() == True:
            print 'gpdb is down. ', comment
            return True
        else:
            print 'gpdb is up. ', comment
            self.errorCnt['gpdbIsDown'] = 1 + self.errorCnt.get('gpdbIsDown')
            return False

    #
    # Stop the database if already started
    #
    def SetGpdbDown(self):
        rc = True
        if self.GpdbDown() == False:
            print 'gpdb is up. Shut it down.'
            rc = self.StopGpdb()
            if rc == True:
                print 'gpdb is stopped.'
            else:
                self.errorCnt['setGpdbDownErrorCnt'] = 1 + self.errorCnt.get('setGpdbDownErrorCnt', 0) 
                #self.errorCnt['setGpdbDownErrorCnt'] += 1
                print 'ERRORFOUND: Unable to shut down gpdb setGpdbDownErrorCnt %d' % (self.errorCnt['setGpdbDownErrorCnt'])
        else:
            print 'gpdb is already down'
        return rc

    #
    # Start the database if not already started
    #
    def SetGpdbUp(self):
        rc = True
        if self.GpdbDown() == True:
            print 'gpdb is down. Start it.'
            rc = self.StartGpdb()
            if rc == True:
                print 'gpdb is start.'
            else:
                self.errorCnt['setGpdbUpErrorCnt'] = 1 + self.errorCnt.get('setGpdbUpErrorCnt', 0) 
                print 'ERRORFOUND: Unable to start gpdb setGpdbUpErrorCnt %d' % (self.errorCnt['setGpdbUpErrorCnt'])
        else:
            print 'gpdb is already up'
        return rc
    '''

    #
    # Get error cnt based on the key
    #
    def GetErrorCnt(self, key):
            return self.errorCnt.get(key, -1)

    #
    # Start the database in interactive mode and answer yes to start the database
    #
    def GpdbStartNoArgYes(self):
        return self.RunCommand("echo y | gpstart", self.gpstartPattern)

    #
    # Start the database in interactive mode and answer no to not start the database
    #
    def GpdbStartNoArgNo(self):
        return self.RunCommand("echo n | gpstart", self.gpstartNoArgNoPattern)

    #
    # ??? -q
    #
    def GpdbStartNoScreenOutputNo(self):
        return self.RunCommand("echo n | gpstart -q", self.gpstartNoScreenOutputPattern)
    #
    # ??? -q
    #
    def GpdbStartNoScreenOutputYes(self):
        return self.RunCommand("echo y | gpstart -q", self.gpstartNoScreenOutputPattern)

###
def Main():
    gpdbSystem = GpdbSystem()
    print gpdbSystem


#
# Input argument
#
def ProcessOption():
    parser = OptionParser()
    parser.add_option("-D", "--debug", action="store_true", dest="debug", default=False, help="Debug print out more detail log messages.")
    (options, args) = parser.parse_args()
    return options


##########################
if __name__ == '__main__':
    options = ProcessOption()
    Main()
    if not options.count:
        options.count = 1

