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

from mpp.models import SQLTestCase
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL

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
    gp_interconnect_setup_timeout = "gp_interconnect_setup_timeout"
    gp_interconnect_timer_checking_period = "gp_interconnect_timer_checking_period"
    gp_interconnect_timer_period = "gp_interconnect_timer_period"
    gp_interconnect_type = "gp_interconnect_type"
    common_sql = 'common/'
    
    @classmethod
    def setUpClass(cls):
        pass

    def __init__(self, methodName):
        super(UDPICFCTestCases, self).__init__(methodName)
        self.infer_metadata()

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

    def test_queue_depth_combination_loss(self):
        try:
            out = self.checkGUC(self.gp_interconnect_queue_depth)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_snd_queue_depth)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_queue_depth + " or " + self.gp_interconnect_snd_queue_depth + " not defined")

        gucValue_recv = self.getGUCvalue(self.gp_interconnect_queue_depth)
        gucValue_snd = self.getGUCvalue(self.gp_interconnect_snd_queue_depth)

        result = self.run_sql_file(local_path(self.common_sql + str(self._testMethodName) + '.sql'), 
                                local_path(self.common_sql + str(self._testMethodName) + '.ans'))
	
        self.setGUCvalue(self.gp_interconnect_queue_depth, gucValue_recv)
        self.setGUCvalue(self.gp_interconnect_snd_queue_depth, gucValue_snd)

        self.assertTrue(result)
    
    def test_queue_depth_combination_capacity(self):
        try:
            out = self.checkGUC(self.gp_interconnect_queue_depth)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_snd_queue_depth)
            self.assertTrue(len(out) > 4)
            out = self.checkGUC(self.gp_interconnect_fc_method)
            self.assertTrue(len(out) > 4)
        except:
            self.skipTest("GUC " + self.gp_interconnect_queue_depth + " or " + self.gp_interconnect_snd_queue_depth + " or " + self.gp_interconnect_fc_method + " not defined")
       
        gucValue_recv = self.getGUCvalue(self.gp_interconnect_queue_depth)
        gucValue_snd = self.getGUCvalue(self.gp_interconnect_snd_queue_depth)        
        gucValue_fc = self.getGUCvalue(self.gp_interconnect_fc_method)
 
        result = self.run_sql_file(local_path(self.common_sql + str(self._testMethodName) + '.sql'), 
                                local_path(self.common_sql + str(self._testMethodName) + '.ans'))

        self.setGUCvalue(self.gp_interconnect_queue_depth, gucValue_recv)
        self.setGUCvalue(self.gp_interconnect_snd_queue_depth, gucValue_snd)
        self.setGUCvalue(self.gp_interconnect_fc_method, gucValue_fc)

        self.assertTrue(result)
