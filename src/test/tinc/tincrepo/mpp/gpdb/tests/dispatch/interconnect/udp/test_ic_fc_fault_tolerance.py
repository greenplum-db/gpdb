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

import fnmatch
import os
import tinctest
import platform

from gppylib.commands.base import Command, CommandResult
from mpp.models import SQLTestCase
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from time import sleep

'''
UDP ic flow control sql tests
'''

def runShellCommand( cmdstr, cmdname = 'shell command'):
    """
    Executes a given command string using gppylib.Command. Definite candidate for a move to
    tinctest.lib.
    @param cmdname - Name of the command
    @param cmdstr - Command string to be executed
    """
    cmd = Command(cmdname, cmdstr)
    tinctest.logger.info('Executing command: %s :  %s' %(cmdname, cmdstr))
    cmd.run()
    result = cmd.get_results()
    tinctest.logger.info('Finished command execution with return code ' + str(result.rc))
    tinctest.logger.debug('stdout: ' + result.stdout)
    tinctest.logger.debug('stderr: ' + result.stderr)
    return (result.stdout, result.rc);

class UDPICFCTestCases(SQLTestCase):
    """
    @product_version gpdb: [4.3-]
    """

    faultTolerance_sql = 'faultTolerance/'
    cluster_platform = ''
    
    @classmethod
    def setUpClass(cls):
        pass

    def __init__(self, methodName):
        super(UDPICFCTestCases, self).__init__(methodName)
        self.infer_metadata()
        (cur_platform,version, state) = platform.linux_distribution()
        self.cluster_platform = cur_platform

    def infer_metadata(self):
        intended_docstring = ""
        sql_file = local_path(self.faultTolerance_sql + str(self._testMethodName) + '.sql')
        with open(sql_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.find('--') != 0:
                    break
                intended_docstring += line[2:].strip()
                intended_docstring += "\n"
                line = line[2:].strip()
                if line.find('@') != 0:
                    continue
                line = line[1:]
                (key, value) = line.split(' ', 1)
                self._metadata[key] = value

        self.gpdb_version = self._metadata.get('gpdb_version', None)

    def checkGUC(self, name):
        if (len(name) <1):
            return -1
        cmd = "show " + name
        out = PSQL.run_sql_command(cmd)
        return out.split('\n')

    def bringDownNIC(self, host, nic):

        tinctest.logger.info("Bringing down %s:%s ..." % (host,nic))
        
        cmd_str = "gpssh -h " + host + " 'sudo ifdown " + nic + "'"
        (out, rc) = runShellCommand(cmd_str)
        
        print(out)

    def bringUpNIC(self, host, nic):
        
        tinctest.logger.info("Bringing up  %s:%s ..." % (host,nic))
        
        cmd_str = "gpssh -h " + host + " 'sudo ifup " + nic + "'"
        (out, rc) = runShellCommand(cmd_str)
        
        print(out)

    def getHostProcess(self, searchStr):

        # Get one segment name after searching pg_catalog
        cmdsql = 'SELECT hostname, port FROM pg_catalog.gp_segment_configuration WHERE content > -1 AND role=\'p\' AND status=\'u\' ORDER BY dbid DESC LIMIT 1;'
        out = PSQL.run_sql_command(cmdsql)        
        result = out.split('\n')
    
        (hostname,port) = result[3].strip().split('|')
        hostname = hostname.strip()
        port = port.strip()

        # find the target process id
        cmdshell = "gpssh -h " + hostname + " 'ps -ef | grep " + searchStr + "'"
        (out, rc) = runShellCommand(cmdshell)

        print(out)

        result = out.split('\n')

	processID = "" 
        for line in result:
            if(line.find("primary") != -1):
                processID = line.split()[2].strip()
                break                
       
        return (hostname, processID)

    def getHostNIC(self):

        # Get one segment name after searching pg_catalog
        cmdsql = 'SELECT hostname, port FROM pg_catalog.gp_segment_configuration WHERE content > -1 AND role=\'p\' AND status=\'u\' ORDER BY dbid ASC LIMIT 1;'
        out = PSQL.run_sql_command(cmdsql)        
        result = out.split('\n')
    
        (hostname,port) = result[3].strip().split('|')
        hostname = hostname.strip()
        port = port.strip()
        print(hostname)

        # Get the interface IP through /etc/hosts file
        cmd_str = "cat /etc/hosts"
        (out, rc) = runShellCommand(cmd_str)
        result = out.split('\n')
        for line in result:
            if((line.find(hostname) != -1) and (line.find(hostname + "-cm")==-1)):
                ipAddress = line.split()[0].strip() 
        
        # ipAddress contain the last interface ic use
        cmd_str = "gpssh -h " + hostname + " 'ifconfig'"
        (out, rc) = runShellCommand(cmd_str)
        result = out.split('\n')
      
        # find the nic name through the ip address 
        for line in result:
            if(len(line) < 10):
                continue
            if(line.split()[1].find("eth") != -1):
                ethname = line.split()[1].strip()
            if(line.find(ipAddress) != -1):
                break
        print(ethname)

        return (hostname, ethname)

    def killHostProcess(self, hostname, processID):

        cmdshell = "gpssh -h " + hostname + " 'kill -9 " + processID + "'"
        (out, rc) = runShellCommand(cmdshell)
        
        print(out)
        if(rc != 0):
            return False
        return True
    
    def checkOutputPattern(self, outfile, pattern):
        try :
            fin = open ((outfile), "r")
            flines = fin.readlines()
            fin.close()
            buf = ' '.join(flines)

            if  buf.find(pattern) < 0:
                return False 
            return True 

        except IOError:
            return str(IOError)

    # def test_ic_fc_nic_failure(self):

      #  (host, nic) = self.getHostNIC();

      #  cmd_str = "psql -a -f " + local_path(self.faultTolerance_sql + str(self._testMethodName) + '.sql') + " &> output_nic &"
      #  out = runShellCommand(cmd_str)
       
        # sleep 1 sec to ensure that some query related to interconnect is running when NIC down
      #  sleep(1)
      #  self.bringDownNIC(host, nic)

        # sleep 90 secs to ensure the sql return some results before bring NIC back
      #  sleep(90)

      #  self.bringUpNIC(host, nic)

        # Run gprecoverseg to recover all break segments
      #  sleep(60)
        
      #  cmd_segSync = "gpstate -m &> gpsync.log"
      #  out = runShellCommand(cmd_segSync)

      #  cmd_segRecover = "gprecoverseg -a -r &> gprecover.log"
      #  out = runShellCommand(cmd_segRecover)
        
        # Check the output file to see whether the expected error info is returned.
      #  out = self.checkOutputPattern("output_nic", "Lost connection to one or more segments")
      #  self.assertTrue(out)
    
    def test_ic_fc_kill_snd_process(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')

        cmd_str = "psql -a -f " + local_path(self.faultTolerance_sql + str(self._testMethodName) + '.sql') + " &> output_killsnd &"
        out = runShellCommand(cmd_str)
        
        # sleep 1 sec to ensure that some query related to interconnect is running when killing sender process
        sleep(1)    
   
        (hostname, processID) = self.getHostProcess("sender")
	
	if(processID == ""):
		self.skipTest("No sender process found, skip this case")

        self.killHostProcess(hostname, processID)

        # sleep 30 sec to ensure that sql return some reuslts
        sleep(50)    

        # Check the output file to see whether the expected error info is returned.
        out = self.checkOutputPattern("output", "server closed the connection unexpectedly")
        self.assertTrue(out)

    def test_ic_fc_kill_recv_process(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        
        cmd_str = "psql -a -f " + local_path(self.faultTolerance_sql + str(self._testMethodName) + '.sql') + " &> output_killrecv &"
        out = runShellCommand(cmd_str)
        
        # sleep 1 sec to ensure that some query related to interconnect is running when killing receiver process
        sleep(1)    
    
        (hostname, processID) = self.getHostProcess("receiver")
	
	if(processID == ""):
		self.skipTest("No sender process found, skip this case")

        self.killHostProcess(hostname, processID)
        
	# sleep 30 sec to ensure that sql return some reuslts
        sleep(50)    

        # Check the output file to see whether the expected error info is returned.
        out = self.checkOutputPattern("output", "server closed the connection unexpectedly")
        self.assertTrue(out)
