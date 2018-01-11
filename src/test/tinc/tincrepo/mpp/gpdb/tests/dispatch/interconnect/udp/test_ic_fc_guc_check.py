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

from mpp.models import SQLTestCase
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from test_udpic import runShellCommand

'''
UDP ic flow control sql tests
'''

class UDPICFCTestCases(SQLTestCase):
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
    gp_interconnect_timer_checking_period = "gp_interconnect_timer_checking_period"
    gp_interconnect_timer_period = "gp_interconnect_timer_period"
    gucCheck_sql = 'gucCheck/'
    hostlist = []
    hoststr = ''
    cluster_platform = ''
    
    @classmethod
    def setUpClass(cls):
        cmdsql = 'SELECT hostname FROM pg_catalog.gp_segment_configuration WHERE content > -1 AND status = \'u\' GROUP BY hostname ORDER by hostname;'
        ret = PSQL.run_sql_command(cmdsql)
        hostlist = ret.split('\n') 
        if (len(hostlist) < 5):
            raise AssertionError('Get segment host list failed') 
        for i in range(3, len(hostlist)):
            if (hostlist[i].find('(') >= 0):
                break
            cls.hostlist.append(hostlist[i].strip()) 
        for host in cls.hostlist:
            cls.hoststr = cls.hoststr + ' -h ' + host

        '''
        result = runShellCommand('gpssh' + cls.hoststr +  ' \"export PATH=$PATH:/sbin\"') 
        if not result:
            raise AssertionError('gpssh run export PATH=$PATH:/sbin failed')
        '''

    def __init__(self, methodName):
        super(UDPICFCTestCases, self).__init__(methodName)
        self.infer_metadata()
        (cur_platform,version, state) = platform.linux_distribution()
        self.cluster_platform = cur_platform

    def infer_metadata(self):
        intended_docstring = ""
        sql_file = local_path(self.gucCheck_sql + str(self._testMethodName) + '_1.sql')
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

    # Check whether the guc exists
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
    
    # Check the result file to see whether expected string exist
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
   
    # Get the expected sub string in the result file
    def searchOutputPattern(self, outfile, start, end):
        try :
            fin = open ((outfile), "r")
            flines = fin.readlines()
            fin.close()
            buf = ' '.join(flines)

            if  ((buf.find(start) > 0) and (buf.find(end) > 0)):
                return buf[buf.find(start)+len(start):buf.find(end)+1]
            return "" 
        except IOError:
            return str(IOError)

    # Run a specific sql file under kernel model and check result - used to check queue depth and fc method
    # Drop packet for 5 times and check DISORDER message
    def run_sql_under_KM(self,suffix):
        
        result = runShellCommand('gpssh' + self.hoststr +  ' \"sudo dmesg -c\"')
        self.assertTrue(result)
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                            sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2 drop_times=5\"') 
        self.assertTrue(result)
        
        sql_result = self.run_sql_file(local_path(self.gucCheck_sql + str(self._testMethodName) + suffix +'.sql'), 
                                local_path(self.gucCheck_sql + str(self._testMethodName) + suffix + '.ans'))

        result = runShellCommand('gpssh' + self.hoststr + ' \"sudo cat /proc/ickmlog \"' + '> ickm.log') 
        self.assertTrue(result)
    
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)        
  
        # Remove KM before check testing result to make sure KM could be removed if case failed. 
        self.assertTrue(sql_result)
        
        # Check if DISORDER info exist
        check_result = self.checkOutputPattern("ickm.log", "DIS")
        return check_result

    # Run a specific sql file under kernel model and check result - used to check transmit timeout, timer period and min retry times
    # Drop packet for 80 times and return sql result
    def run_sql_under_KM_dropMore(self,suffix):
        
        result = runShellCommand('gpssh' + self.hoststr +  ' \"sudo dmesg -c\"')
        self.assertTrue(result)
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                            sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2 drop_times=80\"') 
        self.assertTrue(result)
        
        sql_file = local_path(self.gucCheck_sql + str(self._testMethodName) + suffix + '.sql')
        self.assertTrue(PSQL.run_sql_file(sql_file))        

        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)        

        out_file = sql_file.replace(".sql",".out")

        return out_file
  
    # This method is used to check the change of gp_interconnect_queue_depth take affect
    def test_check_queue_depth(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_queue_depth)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_snd_queue_depth)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_queue_depth + " or " + self.gp_interconnect_snd_queue_depth + " not defined")
        
        # test on default value
        # expected: DISORDER message found
        check_result_1 = self.run_sql_under_KM("_1")

        # test on min value
        # expected: No DISORDER message found
        check_result_2 = self.run_sql_under_KM("_2")
        
        self.assertTrue(check_result_1 and (not check_result_2))
 
    # This method is used to check the change of gp_interconnect_snd_queue_depth take affect
    def test_check_snd_queue_depth(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_snd_queue_depth)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_fc_method)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_snd_queue_depth + " or " + self.gp_interconnect_fc_method + " not defined")
        
        # test on default value
        # expected: DISORDER message found in capacity mode
        check_result_1 = self.run_sql_under_KM("_1")
        
        # test on min value
        # expected: No DISORDER message found in capacity mode
        check_result_2 = self.run_sql_under_KM("_2")
       
        # Verify whether the result is expected
        self.assertTrue(check_result_1 and (not check_result_2))
   
    # This method is used to check the change of gp_interconnect_fc_method take affect
    def test_check_fc_method(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_snd_queue_depth)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_fc_method)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_snd_queue_depth + " or " + self.gp_interconnect_fc_method + " not defined")
        
        # test on default value - loss
        # expected: DISORDER message found in loss mode
        check_result_1 = self.run_sql_under_KM("_1")
        
        # test on capacity mode
        # expected: No DISORDER message found in capacity mode
        check_result_2 = self.run_sql_under_KM("_2")

        # Verify whether the result is expected
        self.assertTrue(check_result_1 and (not check_result_2))
    
    # This method is used to check the change of gp_interconnect_transmit_timeout take affect
    def test_check_transmit_timeout(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_transmit_timeout)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_min_retries_before_timeout)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_transmit_timeout + " or " + self.gp_interconnect_min_retries_before_timeout + " not defined")
        
        # test on default value
        # expected: query pass
        out_file_1 = self.run_sql_under_KM_dropMore("_1")
        check_result_1 = self.checkOutputPattern(out_file_1, "Failed to send packet")
        
        # Set transmint timeout to 30s, then execute same query
        # expected: query timeout
        out_file_2 = self.run_sql_under_KM_dropMore("_2")
        check_result_2 = self.checkOutputPattern(out_file_2, "Failed to send packet")
        
        # Verify whether the result is expected
        self.assertTrue((not check_result_1) and check_result_2)
    
    # This method is used to check the change of gp_interconnect_min_retries_before_timeout take affect
    def test_check_min_retries(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_transmit_timeout)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_min_retries_before_timeout)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_transmit_timeout + " or " + self.gp_interconnect_min_retries_before_timeout + " not defined")
        
        # test on default value
        # expected: query pass
        out_file_1 = self.run_sql_under_KM_dropMore("_1")
        check_result_1 = self.checkOutputPattern(out_file_1, "Failed to send packet")

        # Set transmint timeout to 30s, then execute same query
        # expected: query timeout
        out_file_2 = self.run_sql_under_KM_dropMore("_2")
        check_result_2 = self.checkOutputPattern(out_file_2, "Failed to send packet")
        
        # Verify whether the result is expected
        self.assertTrue((not check_result_1) and check_result_2)
    
    # This method is used to check the change of gp_interconnect_timer_period take affect
    def test_check_timer_period(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        try:
            out = self.checkGUC(self.gp_interconnect_timer_period)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_min_retries_before_timeout)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_transmit_timeout)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_transmit_timeout + " or " + self.gp_interconnect_min_retries_before_timeout + " or " + self.gp_interconnect_timer_period + " not defined")
        
        # test on default value
        # expected: query fail with a larger retries time
        out_file_1 = self.run_sql_under_KM_dropMore("_1")
        retries_times_1 = int(self.searchOutputPattern(out_file_1, "after ", " retries"))
 
        # Set timer period to max value, then execute same query
        # expected: query fail with a smaller retries time
        out_file_2 = self.run_sql_under_KM_dropMore("_2") 
        retries_times_2 = int(self.searchOutputPattern(out_file_2, "after ", " retries"))
        
        # Verify whether the result is expected
        self.assertTrue(retries_times_1 > retries_times_2)
