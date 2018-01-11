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

GpConfig class is used to view, delete, update persistent GUCs.
"""

############################################################################
# Set up some globals, and import gptest
#    [YOU DO NOT NEED TO CHANGE THESE]
############################################################################
import sys, os, subprocess, inspect
MYD = os.path.abspath( os.path.dirname( __file__ ) )
mkpath = lambda * x: os.path.join( MYD, *x )
UPD = os.path.abspath( mkpath( '..' ) )
if UPD not in sys.path:
    sys.path.append( UPD )

import pprint
import re
import subprocess
try:
    from tinctest.main import TINCException
    from gppylib.commands.base import Command 
#    from cdbfastUtil import PrettyPrint
#    from dictCheck import DictCheck
except Exception, e:
    sys.exit('Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

#####

class GpConfig:
    """
    GpConfig class is used to view, delete, update persistent GUCs.
    @class GpConfig
    
    @organization: DCD Partner Engineering
    @contact: Yi Ren
    @exception: TINCException
    """

    ###
    def __init__(self, host="localhost", user="", configInfo=None):
        """ 
        Constructor for GpConfig
        @param host: hostname
        @param user: username
        @change: Johnny Soedomo - 9/20/11
        Move the gpRemoteCmd creation in init rather
        than in __run, and use the default "localhost" and runtime USER when
        we initialize the object without parameter.
        """
        self.host = host
        if user == "":
            self.user = os.environ.get( "LOGNAME" )
        else:
            self.user = user
        self.gphome = os.environ['GPHOME']
        self.mdd = os.environ['MASTER_DATA_DIRECTORY']
        self.port = os.environ['PGPORT']
        if configInfo:
            self.gphome = configInfo['GPHOME']
            self.mdd = configInfo['MASTER_DATA_DIRECTORY']
            self.port = configInfo['PGPORT']

    ###
    def __run(self, command):
        """
        Run specific cmd on remote machine.
        @param command: Command to be executed on the remote server.
        @return: the executed command's output.
        @change: Johnny Soedomo - 9/20/11
        We cannot just run the command for GPDB cli. We need to source
        the greenplum_path.sh
        @change: Johnny Soedomo - 9/27/11
        Need to set MASTER_DATA_DIRECTORY and PGPORT
        @change: Johnny Soedomo - 10/17/12
        On Solaris, python os.popen is using /bin/sh instead of /bin/bash
        This is a temp fix. Our Solaris run is single node setup.
        Upgrade is the only time that needs to source correctly for GPDB, Master, and Port
        """
        self.gp_rcmd = Command("gpConfig", command, remoteHost=self.host)
        if sys.platform.find("sun") == 0:
            cmd = command
        else:
            cmd = "source %s/greenplum_path.sh; export MASTER_DATA_DIRECTORY=%s; export PGPORT=%s; %s" % (self.gphome, self.mdd, self.port, command)
        self.gp_rcmd.run()
        rc = self.gp_rcmd.results.rc
        out = self.gp_rcmd.results.stdout
        if rc:
            raise TINCException("Unable to connect to segment server %s as user %s" % (self.host, self.user))
        return out

    def getPort(self):
        """
        Get the values of "Port" for all nodes in the cluster.
        @return: a dictionary with context of the cluster as its keys and the port of the cluster as its values.
        """
        master_dict = self.getPortMasterOnly()
        segment_dict = self.getPortSegmentOnly()
        master_dict.update( segment_dict )
        return master_dict

    def getPortMasterOnly(self):
        """
        Get the value of "Port" for the master node in the cluster.
        @return: a dictionary with context of the master as its key and the port of the master as its value.
        """
        cmd= "gpconfig -s %s" % ( "port" )
        output = self.__run(cmd)
        master_pattern = "Context:\s*-1\s*Value:\s*\d+"
        master_value = None
        context_value = None
        for line in output.split('\n'):
            if re.search( master_pattern, line ):
                context_value = int(line.split()[1].strip())
                master_value = int(line.split()[3].strip())

        if master_value == None or \
                context_value == None:
            error_msg= "".join(output)
            raise TINCException(error_msg)
        return { context_value:master_value }

    def getPortSegmentOnly(self):
        """
        Get the values of "Port" for all segment nodes in the cluster.
        @return: a dictionary with context of the segment nodes as its keys and the port of the segment nodes as its values.
        """
        cmd= "PGDATABASE=postgres gpconfig -s %s" % ( "port" )
        output = self.__run(cmd)
        segment_pattern = "Context:\s*\d+\s*Value:\s*\d+"
        segment_value = None
        context_value = None
        ret_dict = { }
        for line in output.split('\n'):
            if re.search( segment_pattern, line ):
                context_value = int(line.split()[1].strip())
                segment_value = int(line.split()[3].strip())
                if context_value != -1:
                    ret_dict[context_value] = segment_value

        if not ret_dict:
            error_msg= "".join(output)
            raise TINCException(error_msg)

        return ret_dict

    def getParameter(self, name ):
        """
        Get the values of the specified parameter for both master node and segment nodes.
        @param name: the parameter name
        @return: the values of the specified parameter for both master node and segment nodes.
        """
        if name.strip().lower() == "port":
            raise TINCException("use getPort to retrieve port information")
        master_value = self.getParameterMasterOnly(name)
        segment_value = self.getParameterSegmentOnly(name)
        return (master_value, segment_value)

    def getParameterMasterOnly(self, name ):
        """
        Get the value of the specified parameter for master node.
        @param name: the parameter name
        @return: the value of the specified parameter for master node.
        """
        if name.strip().lower() == "port":
            raise TINCException("use getPortMasterOnly to retrieve port information")
        master_value = None
        master_sign = "Master  value:"
        index = -1
        cmd= "gpconfig -s %s" % ( name )
        output = self.__run(cmd)
        len_output = len(output)

        for line in output.split("\n"):
            if line.find(master_sign)!=-1:
                master_value = line.split(" ")[-1]
    
        if master_value == None:
            error_msg = ""
            error_msg = error_msg.join(output)
            raise TINCException(error_msg)

        return master_value.strip()

    def getParameterSegmentOnly(self, name ):
        """
        Get the value of the specified parameter for segment nodes.
        @param name: the parameter name
        @return: the value of the specified parameter for segment nodes.
        """
        if name.strip().lower() == "port":
            raise TINCException("use getPortSegmentOnly to retrieve port information")
        segment_value = None
        segment_sign = "Segment value:"
        index = -1
        cmd= "gpconfig -s %s" % ( name )
        output = self.__run(cmd)
        len_output = len(output)

        for line in output.split("\n"):
            if line.find(segment_sign)!=-1:
                segment_value = line.split(" ")[-1]
    
        if segment_value == None:
            error_msg = ""
            error_msg = error_msg.join(output)
            raise TINCException(error_msg)
        return segment_value.strip()

    def __performCmd( self, cmd ):
        output = self.__run(cmd)
        success_sign="completed successfully"
        len_output = len(output)
        error_msg = ""

        for line in output.split("\n"):
            if line.find(success_sign)!=-1:
                return
    
        error_msg = error_msg.join(output)
        raise TINCException(error_msg)

    def setParameterMasterOnly(self, name, master_value, option="" ):
        """
        Set the value of the specified parameter for the master node.
        The user needs to restart the database to make the changes to take effect.
        @param name: the parameter name.
        @param master_value: the value to be set.
        @param option: default is ""
        @return: None
        """
        cmd= "gpconfig -c %s -v %s --masteronly %s" % (name, master_value, option)
        self.__performCmd( cmd )

    def setParameter(self, name, master_value, segment_value, option="" ):
        """
        Set the value of the specified parameter for all the nodes.
        The user needs to restart the database to make the changes to take effect.
        @param name: the parameter name.
        @param master_value: the master value to be set.
        @param segment_value: the segment value to be set.
        @param option: default is ""
        @return: None
        """
        cmd= "gpconfig -c %s -v %s -m %s %s" % (name, segment_value, master_value, option)
        self.__performCmd( cmd )

    def setParameterSegmentOnly(self, name, segment_value, option="" ):
        """
        Set the value of the specified parameter for segment nodes.
        The user needs to restart the database to make the changes to take effect.
        @param name: the parameter name
        @param segment_value: the value to be set
        @param option: default is ""
        @return: None
        """
        master_value = self.getParameterMasterOnly(name)
        self.setParameter( name, master_value, segment_value, option)

    def removeParameter(self, name ):
        """
        Remove the specified parameter for the master node and segment nodes.
        After DB restart, that parameter will be reset to default value.
        @param name: the parameter name
        @return: None
        """
        cmd= "gpconfig -r %s" % (name)
        self.__performCmd( cmd )

    def removeParameterMasterOnly(self, name ):
        """
        Remove the specified parameter for the master node.
        The user needs to restart the database to make the changes to take effect.
        After database restarts, that parameter will be reset to default value.
        @param name: the parameter name
        @return: None
        """
        cmd= "gpconfig -r %s --masteronly" % (name)
        self.__performCmd( cmd )

    def removeParameterSegmentOnly(self, name ):
        """
        Remove the specified parameter for the segment nodes.
        The user needs to restart the database to make the changes to take effect.
        After database restarts, that parameter will be reset to default value.
        @param name: the parameter name
        @return: None
        """
        master_value = self.getParameterMasterOnly( name )
        self.removeParameter( name )
        self.setParameterMasterOnly( name, master_value)

    def verifyParameterMasterOnly(self, name, master_value ):
        """
        Verify if the value of the specified parameter on master node is as expected.
        @param name: the parameter name
        @param master_value: the value to be verified
        @return: True if the value of the specified parameter for current setting is the same as the expected value
                 False otherwise
        """
        curr_value = self.getParameterMasterOnly( name )

        if curr_value == master_value.strip():
            return True
        else:
            return False

    def verifyParameterSegmentOnly(self, name, segment_value ):
        """
        Verify if the value of the specified parameter on segment nodes is as expected.
        @param name: the parameter name
        @param master_value: the value to be verified
        @return: True if the value of the specified parameter for current setting is the same as the expected value
                 False otherwise
        """
        curr_value = self.getParameterSegmentOnly( name )

        if curr_value == segment_value.strip():
            return True
        else:
            return False

    def verifyParameter(self, name, master_value, segment_value ):
        """
        Verify if the values of the specified parameter on master node and segment nodes are as expected.
        @param name: the parameter name
        @param master_value: the expected value to be verified on master node
        @param segment_value: the expected value to be verified on segment ndoes
        @return: True if the values of the specified parameter for current setting are the same as the expected values for master node and segment nodes respectively
                 False otherwise
        """
        return (self.verifyParameterSegmentOnly( name, segment_value) \
                and self.verifyParameterMasterOnly( name, master_value))

    ###
    def GucChanged(self, pastDict, currentDict):
        """
        Identify the list of GUC that has changed
        @param pastDict: Initial dictionary
        @param currentDict: Current dictionary
        """
        dictCheck = DictCheck(pastDict, currentDict)
        dictChange = dictCheck.DictChanged()
        return dictChange

    ###
    def GucAdded(self, pastDict, currentDict):
        """
        Identify the list of GUC that has been added
        @param pastDict: Initial dictionary
        @param currentDict: Current dictionary
        """
        dictCheck = DictCheck(pastDict, currentDict)
        dictAdded = dictCheck.DictAdded()
        return dictAdded

    ###
    def GucRemoved(self, pastDict, currentDict):
        """
        Identify the list of GUC that has been removed
        @param pastDict: Initial dictionary
        @param currentDict: Current dictionary
        """
        dictCheck = DictCheck(pastDict, currentDict)
        dictRemoved = dictCheck.DictRemoved()
        return dictRemoved
    
    ###
    def GetMasterGuc(self, exludeList=None):
        """
        @param exludeList: Exclude the list of keys from result
        """
        gucData = {}
        gucCheck = re.compile(r'^(\S+)\s*\|\s*(\S+)\s*\|')
        cmd = "psql template1 -c 'select * from pg_settings;'"
        p = subprocess.Popen(cmd, shell=True, stdin=None, stdout=subprocess.PIPE)
        for line in p.stdout.split("\n"):
            line = line.strip()
            match = gucCheck.match(line)
            if match:
                key, value = match.groups()
                if not key in exludeList:
                    gucData[key] = value
        return gucData

    ###
    def GetSegmentGuc(self, exludeList=None):
        """
        Query the database and extract from the view the list of GUC settings for the segment servers
        @param exludeList: Exclude the list of keys from result
        """
        gucData = {}
        gucCheck = re.compile(r'^(\d+)\s*\|\s*(\S+)\s*\|\s*(\S+|\S+ \S+)?$')
        cmd = "psql template1 -c 'select * from gp_toolkit.gp_param_settings();'"
        p = subprocess.Popen(cmd, shell=True, stdin=None, stdout=subprocess.PIPE)
        for line in p.stdout.split("\n"):
            line = line.strip()
            match = gucCheck.match(line)
            if match:
                segid, key, value = match.groups()
                if not key in exludeList:
                    # Create segment id key
                    if not segid in gucData:
                        gucData[segid] = {}
                    gucData[segid][key] = value
        return gucData

if __name__ == '__main__':
    pass
    '''
    configInfo = {}
    configInfo['MASTER_DATA_DIRECTORY'] = "/data/wongk8/data/main/master/gpseg-1"
    configInfo['GPHOME'] = "/data/wongk8/data/main/greenplum-db"
    configInfo['PGPORT'] = 20000
    gpConfig = GpConfig(configInfo=configInfo)
    excludeList = ['port', 'gpperfmon_port', 'gp_session_id']
    gucData = gpConfig.GetMasterGuc(excludeList)
    gucData2 = gucData.copy()
    PrettyPrint('main', gucData)
    diff = gpConfig.GucChanged(gucData, gucData2)
    PrettyPrint('master diff', diff)
    # Segments
    excludeList = ['gp_contentid', 'gp_dbid', 'port']
    gucData = gpConfig.GetSegmentGuc(excludeList)
    PrettyPrint('segment', gucData)
    gucData1 = gucData['1']
    gucData2 = gucData['2']
    diff = gpConfig.GucChanged(gucData1, gucData2)
    PrettyPrint('segment diff', diff)
    '''
