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
import tinctest
from mpp.gpdb.tests.storage.fts.fts_transitions.fts_transitions import FTSTestCase

class FtsTransitionsPart01(FTSTestCase):
    ''' State of FTS at different fault points
    '''
    
    def __init__(self, methodName):
        super(FtsTransitionsPart01,self).__init__(methodName)

    def test_filerep_sync_ct(self):
        '''
        @data_provider sync_ct_tests
        '''
        fault_name = self.test_data[1][0]
        fault_type = self.test_data[1][1]
        fault_role = self.test_data[1][2]
        filerep_state = self.test_data[1][3]
        filerep_role = self.test_data[1][4]

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s " % self.test_data[0][1])  
        tinctest.logger.info("\n ===============================================")
        self.filerep_sync_ct(fault_name, fault_type, fault_role, filerep_state, filerep_role)
     

@tinctest.dataProvider('sync_ct_tests')
def test_sync_ct():
    data = {'test_01_mirror_sync_postmaster_reset_filerep_sender': ['filerep_sender','panic','mirror','ct','mirror'],
            'test_02_mirror_sync_postmaster_reset_filerep_receiver': ['filerep_receiver','panic','mirror','ct','mirror'],
            'test_03_mirror_sync_postmaster_reset_filerep_flush': ['filerep_flush','panic','mirror','ct','mirror'],
            'test_04_mirror_sync_postmaster_reset_filerep_consumer': ['filerep_consumer','panic','mirror','ct','mirror'],
            'test_10_mirror_sync_filerep_process_error_failover': ['filerep_sender','error','mirror','ct','primary'],
            'test_11_primary_sync_filerep_process_error_failover': ['filerep_sender','error','primary','ct','mirror'],
            'test_14_mirror_sync_dealock_failover': ['filerep_sender','infinite_loop','mirror','ct','primary'],
            'test_15_primary_sync_deadlock_failover': ['filerep_sender','infinite_loop','primary','ct','mirror'],
            'test_16_primary_sync_filerep_network_failover': ['filerep_receiver','fault','primary','ct','mirror'],
            'test_18_primary_sync_process_missing_failover': ['postmaster','panic','primary','ct','mirror'],
            'test_42_mirror_sync_filerep_network': ['filerep_receiver','fault','mirror','ct','mirror'],
            'test_43_mirror_sync_system_io_failover': ['filerep_flush', 'error', 'mirror','ct','mirror'],
            'test_44_mirror_sync_postmaster_missing_failover': ['postmaster', 'panic', 'mirror','ct','mirror'],
            'test_postmaster_reset_mpp13689': ['filerep_receiver','fatal','mirror','ct','primary']}
    return data
