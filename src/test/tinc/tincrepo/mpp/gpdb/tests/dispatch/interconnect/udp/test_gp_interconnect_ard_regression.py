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
import random
import time
import subprocess
import platform

from mpp.models import SQLTestCase
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command

'''
UDP ic Bugs for verificaton
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
    if result.rc != 0:
        return False
    return True

class UDPICARDBFVCases(SQLTestCase):
    """
    @product_version gpdb: [4.3-]
    """

    # GUC 
    gp_interconnect_queue_depth = "gp_interconnect_queue_depth"
    gp_interconnect_snd_queue_depth = "gp_interconnect_snd_queue_depth"
    gp_interconnect_min_retries_before_timeout = "gp_interconnect_min_retries_before_timeout"
    gp_interconnect_transmit_timeout = "gp_interconnect_transmit_timeout"
    gp_interconnect_cache_future_packets = "gp_interconnect_cache_future_packets"
    gp_interconnect_default_rtt = "gp_interconnect_default_rtt"
    gp_interconnect_fc_method = "gp_interconnect_fc_method"
    gp_interconnect_hash_multiplier = "gp_interconnect_hash_multiplier"
    gp_interconnect_min_rto = "gp_interconnect_min_rto"
    gp_interconnect_setup_timeout = "gp_interconnect_setup_timeout"
    gp_interconnect_timer_checking_period = "gp_interconnect_timer_checking_period"
    gp_interconnect_timer_period = "gp_interconnect_timer_period"
    gp_interconnect_type = "gp_interconnect_type"
    common_sql = 'common/'

    hostlist = []
    hoststr = ''
    log_str = ''
    cluster_platform = ''

    @classmethod
    def setUpClass(cls):
        sql = "SELECT hostname FROM pg_catalog.gp_segment_configuration WHERE content > -1"  \
                  "AND status = 'u' GROUP BY hostname ORDER by hostname;"
        psql_cmd = "psql -c " + '"' + sql + '"' 
        psqlProcess =  subprocess.Popen(psql_cmd, shell = True, stdout = subprocess.PIPE)
        ret = psqlProcess.stdout.read().split('\n')
        if (len(ret) < 5): 
            raise AssertionError('Get segment host list failed')
        cls.hoststr = ''.join(['-h %s '%host for host in ret[2:] if host != '' and host.find('(') < 0] )

        if(os.path.exists(local_path('log/'))==False):
            os.mkdir(local_path('log/'))
        cls.log_str='gpssh ' + cls.hoststr + ' \"sudo cat /proc/ickmlog\"' + '>>' + local_path('log/')


    def __init__(self, methodName):
        super(UDPICARDBFVCases, self).__init__(methodName)
        self.infer_metadata()
        (cur_platform,version, state) = platform.linux_distribution()
        self.cluster_platform = cur_platform
        
    def infer_metadata(self):
        intended_docstring = ""
        sql_file = local_path(self.common_sql + str(self._testMethodName) + '.sql')
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
    
    # Get the GUC value before change it 
    def getGUCvalue(self, name):
        if(len(name) < 1):
            return -1
        cmd = "show " + name 
        out = PSQL.run_sql_command(cmd)
        result = out.split('\n')
        return result[3].strip()

    # Reset the GUC value after test case finish
    def setGUCvalue(self, name, value):
        if(len(name) < 1):
            return -1
        cmd = "set " + name + " = " + value
        out = PSQL.run_sql_command(cmd)


    def test_gp_interconnect_fc_ard_142(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_min_retries_before_timeout)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_transmit_timeout)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_fc_method)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_min_retries_before_timeout + " or " + self.gp_interconnect_transmit_timeout + " or " +  self.gp_interconnect_fc_method + " not defined")


        result = runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                              sudo insmod ickm.ko ict_type=0x101 seq_array=2 drop_times=80\"')
        self.assertTrue(result)
        
        sql_file = local_path(self.common_sql + str(self._testMethodName) + '.sql');
        self.assertTrue(PSQL.run_sql_file(local_path(sql_file)))        
        out_file = sql_file.replace(".sql",".out")
        test_ret = "Failed to send packet (seq 2) to" in open(out_file).read() and "retries in 40 seconds" in open(out_file).read()
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log' )
        result = runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"')

        self.assertTrue(result)  
        self.assertTrue(ret_log)
        self.assertTrue(test_ret)
