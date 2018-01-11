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

class FtsTransitionsPart02(FTSTestCase):
    ''' State of FTS at different fault points
    '''
    
    def __init__(self, methodName):
        super(FtsTransitionsPart02,self).__init__(methodName)
    
    def test_gpstate_resync_object_count(self):
        '''
        @product_version gpdb: [4.3.5.0-]
        '''
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: GPSTATE RESYNC OBJ COUNT ")
        tinctest.logger.info("\n ===============================================")
        self.gpstate_resync_object_count()

    def test_ctchecksum_resync(self):
        tinctest.logger.info("\n ======================================================")
        tinctest.logger.info("\n Starting New Test: CT Checksum corruption inc resync")
        tinctest.logger.info("\n ======================================================")
        self.checksum_ct_logfile_recoverseg()

    def test_ctchecksum_restart_after_corrupt_inline(self):
        tinctest.logger.info("\n ===================================================================================")
        tinctest.logger.info("\n Starting New Test: CT Checksum corruption restart DB after corruping CT file inline")
        tinctest.logger.info("\n ===================================================================================")
        self.checksum_ct_logfile_restartdb_corrupt_inline()

    def test_ctchecksum_restart_after_corrupt_by_appending_partial_page(self):
        tinctest.logger.info("\n ============================================================================================")
        tinctest.logger.info("\n Starting New Test: CT Checksum corruption restart DB after appending partial page to CT file")
        tinctest.logger.info("\n ============================================================================================")
        self.checksum_ct_logfile_restartdb_corrupt_by_appending_partial_page()

    def test_ctchecksum_resync_restart(self):
        tinctest.logger.info("\n ======================================================")
        tinctest.logger.info("\n Starting New Test: CT Checksum corruption resync and restart DB")
        tinctest.logger.info("\n ======================================================")
        self.checksum_ct_logfile_recoverseg_restartdb()

    def test_sync_postmaster_reset(self):
        '''
        @data_provider sync_postmaster_tests
        '''
        fault_name = self.test_data[1][0]
        fault_type = self.test_data[1][1]
        fault_role = self.test_data[1][2]
        filerep_state = self.test_data[1][3]
        filerep_role = self.test_data[1][4]

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s " % self.test_data[0][1])
        tinctest.logger.info("\n ===============================================")
        self.sync_postmaster_reset(fault_name, fault_type, fault_role, filerep_state, filerep_role)

    def test_primary_sync_mirror_cannot_keepup_failover(self):
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: test_primary_sync_mirror_cannot_keepup_failover")
        tinctest.logger.info("\n ===============================================")
        self.primary_sync_mirror_cannot_keepup_failover()

    def test_21_change_tracking_transition_failover(self):

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: test_21_change_tracking_transition_failover" )
        tinctest.logger.info("\n ===============================================")
        self.change_tracking_transition_failover()

    def test_mirror_resync_postmaster_reset_filerep_fault(self):
        '''
        @data_provider filerep_fault
        '''
        filerep_fault = self.test_data[1][0]

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s " % self.test_data[0][1])
        tinctest.logger.info("\n ===============================================")
        self.mirror_resync_postmaster_reset_filerep_fault(filerep_fault)
   
    def test_wait_for_shutdown_before_commit(self):
        '''
        @product_version gpdb: [4.3.4.0-]
        '''
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: test_wait_for_shutdown_before_commit" )
        tinctest.logger.info("\n ===============================================")
        self.wait_for_shutdown_before_commit()



@tinctest.dataProvider('sync_postmaster_tests')
def test_postmaster_reset():
    data = {'test_05_primary_sync_postmaster_reset_filerep_sender': ['filerep_sender','panic','primary','sync1','mirror'],
            'test_06_primary_sync_postmaster_reset_filerep_receiver': ['filerep_receiver','panic','primary','sync1','mirror'],
            'test_07_primary_sync_postmaster_reset_checkpoint': ['checkpoint','panic','primary','sync1','mirror'],
            'test_08_primary_sync_postmaster_reset_filerep_flush': ['filerep_flush','panic','primary','sync1','mirror'],
            'test_09_primary_sync_postmaster_reset_filerep_consumer': ['filerep_consumer','panic','primary','sync1','mirror'],
            'test_postmaster_reset_mpp13971': ['filerep_flush','panic','primary','sync1','mirror'] }
    return data
               
@tinctest.dataProvider('filerep_fault')
def test_filerep_fault():
    data = {'test_23_mirror_resync_postmaster_reset_filerep_sender': ['filerep_sender'],
            'test_24_mirror_resync_postmaster_reset_filerep_receiver': ['filerep_receiver'],
            'test_25_mirror_resync_postmaster_reset_filerep_flush': ['filerep_flush']}
    return data
