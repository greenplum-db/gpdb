"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

class FtsTransitionsPart03(FTSTestCase):
    ''' State of FTS at different fault points
    '''
    
    def __init__(self, methodName):
        super(FtsTransitionsPart03,self).__init__(methodName)
    
    def test_primary_resync_postmaster_reset_with_faults(self):
        '''
        @data_provider pr_faults
        '''
        filerep_fault = self.test_data[1][0]
        fault_type = self.test_data[1][1]
        filerep_role = self.test_data[1][2]
        gpstate_role = self.test_data[1][3]
        gprecover = self.test_data[1][4]
        
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s " % self.test_data[0][1])
        tinctest.logger.info("\n ===============================================")
        self.primary_resync_postmaster_reset_with_faults(filerep_fault, fault_type, filerep_role, gpstate_role, gprecover)

    def test_mirror_resync_postmaster_reset_with_faults(self):

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: test_mirror_resync_postmaster_reset_with_faults ")
        tinctest.logger.info("\n ===============================================")
        self.mirror_resync_postmaster_reset_with_faults()

@tinctest.dataProvider('pr_faults')
def test_pr_faults():

    data = {'test_27_primary_resync_postmaster_reset_checkpoint': ['checkpoint','panic', 'primary', 'mirror', 'no'],
            'test_28_primary_resync_postmaster_reset_filerep_flush': ['filerep_flush','panic','primary', 'mirror', 'no'],
            'test_29_primary_resync_postmaster_reset_filerep_consumer': ['filerep_consumer','panic', 'primary', 'mirror', 'no'],
            'test_30_mirror_resync_process_missing_failover': ['filerep_sender','error', 'mirror', 'mirror', 'yes'],
            'test_31_primary_resync_process_missing_failover': ['filerep_sender','error', 'primary', 'primary', 'yes'],
            'test_32_mirror_resync_deadlock_failover': ['filerep_sender', 'infinite_loop', 'mirror', 'mirror', 'yes'],
            'test_33_primary_resync_deadlock_failover': ['filerep_sender', 'infinite_loop', 'primary', 'primary', 'yes'],
            'test_34_primary_resync_filerep_network_failover': ['filerep_consumer', 'panic', 'mirror', 'mirror', 'yes'],
            'test_36_primary_resync_postmaster_missing_failover': ['postmaster', 'panic', 'primary', 'mirror', 'no'],
            'test_37_primary_resync_system_failover': ['filerep_resync_worker_read', 'fault', 'primary', 'mirror', 'yes'],
            'test_38_primary_resync_mirror_cannot_keepup_failover': ['filerep_receiver', 'sleep', 'primary', 'mirror', 'no'],
            'test_39_mirror_resync_filerep_network': ['filerep_receiver', 'fault', 'mirror', 'primary', 'yes'],
            'test_40_mirror_resync_system_failover': ['filerep_flush', 'fault', 'mirror', 'primary', 'yes'],
            'test_41_mirror_resync_postmaster_missing_failover': ['postmaster', 'panic', 'mirror', 'primary', 'yes'],
            'test_46_primary_resync_postmaster_reset_filerep_sender': ['filerep_sender', 'panic', 'primary', 'primary', 'yes']}
            
    return data
