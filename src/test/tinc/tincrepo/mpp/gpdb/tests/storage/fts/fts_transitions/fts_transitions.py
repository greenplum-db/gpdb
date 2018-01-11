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

import tinctest
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.config import GPDBConfig
from tinctest.models.scenario import ScenarioTestCase
from mpp.models import MPPTestCase

class FTSTestCase(ScenarioTestCase, MPPTestCase):
    ''' FTS transitions with filerep_faults and other cluster changes'''
    
    def __init__(self, methodName):
        super(FTSTestCase,self).__init__(methodName)

    def setUp(self):
        self.fileutil = Filerepe2e_Util()
        self.fileutil.inject_fault(f='all', m='async', y='reset', r='primary_mirror', H='ALL')
        super(FTSTestCase, self).setUp()

    def tearDown(self):
        super(FTSTestCase, self).tearDown()

    def check_system(self):
        '''
        Check state of system and exit if not in sync & up state
        '''
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_system')
        self.test_case_scenario.append(test_case_list0)

    def run_gprecover_and_validation(self):
        '''
        Run gprecover and validation
        '''
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list0, serial=True)

    def fts_test_run(self, filerep_role, filerep_state):

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        self.test_case_scenario.append(test_case_list1, serial=True)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate', [filerep_role, filerep_state]))
        self.test_case_scenario.append(test_case_list3)

        self.run_gprecover_and_validation()
        
    def filerep_sync_ct(self, fault_name, fault_type, fault_role, filerep_state, filerep_role):
        '''
        sync_ct_tests with fierep faults
        '''
        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', [fault_name, fault_type, fault_role]))
        self.test_case_scenario.append(test_case_list0)
        
        self.fts_test_run(filerep_role, filerep_state)

        if fault_name == 'postmaster':
            test_case_list6 = []
            test_case_list6.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db')
            self.test_case_scenario.append(test_case_list6)
        

    def sync_postmaster_reset(self, fault_name, fault_type, fault_role, filerep_state, filerep_role):
        '''
        sync_postmaster_reset_tests with fierep faults
        ''' 
        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', [fault_name, fault_type, fault_role]))
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml')
        self.test_case_scenario.append(test_case_list1, serial=True)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_recoverseg_if_ct')
        test_case_list2.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list2, serial=True)

        test_case_list3 = []
        test_case_list3.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate', [filerep_role, filerep_state]))
        self.test_case_scenario.append(test_case_list3)

        if fault_name == 'checkpoint':
                test_case_list3_1 = []
                test_case_list3_1.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', [fault_name, 'reset', fault_role]))
                self.test_case_scenario.append(test_case_list3_1)

    def primary_sync_mirror_cannot_keepup_failover(self):

        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.set_gpconfig', ['gp_segment_connect_timeout', 20, False]))
        # Sleep for some time greater than *gp_segment_connect_timeout* which
        # should trigger no response received from mirror for that much time and
        # help validate the scenario.
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_receiver', 'sleep', 'primary'], {'occurence':0, 'sleeptime':40}))
        self.test_case_scenario.append(test_case_list0, serial=True)

        # *heartbeat* between primary and mirror triggers every minute. It will
        # get blocked due to above fault. As the time it blocks exceeds
        # gp_segment_connect_timeout, mirror should get marked as down and
        # primary transition to CT.

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list3.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_receiver', 'reset', 'primary'], {'occurence':0, 'sleeptime':40}))
        test_case_list3.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.reset_gpconfig', ['gp_segment_connect_timeout', False]))
        test_case_list3.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml')
        self.test_case_scenario.append(test_case_list3, serial=True)

        test_case_list4 = []
        test_case_list4.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate', ['primary','ct']))
        self.test_case_scenario.append(test_case_list4)

        self.run_gprecover_and_validation()

    def change_tracking_transition_failover(self):

        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_transition_to_change_tracking', 'panic', 'primary']))
        self.test_case_scenario.append(test_case_list1)

        self.fts_test_run('primary','ct')

    # This test creates multiple blocks to CT_FULL file, corrupts first block
    # and validates incremental resync detects corrupted CT file, fails and full
    # resync brings cluster back in sync. In this case mutlipe blocks are
    # required to be created to CT_FULL file, to deterministically corrupt first
    # block without worrying about it getting overwritten later.
    def checksum_ct_logfile_recoverseg(self):
        self.check_system()

        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_before_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_ct')
        # This allows to generate work-load to create multiple blocks in CT_FULL file
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_checksum_ct_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.corrupt_ct_logfile')

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.full_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list0, serial=True)

    # Gets cluster in CT state, corrupts CT file and restart DB. Validates DB
    # doesn't go into rolling PANICs but instead moves to CT disabled
    # state. Incremental resync fails and only full resync brings it back to
    # sync state.
    def checksum_ct_logfile_restartdb_corrupt_inline(self):
        self.check_system()

        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_before_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_ct')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.corrupt_ct_logfile')

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.full_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list0, serial=True)

    # In this test, instead of corrupting the perfect CT_BLOCK_SIZE block,
    # inline. Appends dirty data at end of file to simulate partial write
    # case. This scenario restart detects issue with CT files, moves to CT
    # disabled state, then validates incremental resync fails.
    def checksum_ct_logfile_restartdb_corrupt_by_appending_partial_page(self):
        self.check_system()

        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_before_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.corrupt_ct_logfile', ['LetsCorruptFileByWritingThisPartialCTBlock', 0, False]))

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        # Make sure further attempts of incremental recovery also fail.
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        # Only final recovery should bring us back to sync state.
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.full_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list0, serial=True)

    # In this scenario, incremental resync detects issue with CT files, moves to
    # CT disabled state. Then validates on restart, it remains in CT disabled
    # state, incremental resync fails.
    def checksum_ct_logfile_recoverseg_restartdb(self):
        self.check_system()

        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_before_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_ct')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_checksum_ct_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.corrupt_ct_logfile')

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')

        # Restart and make sure DB stays in CT and incremental recovery is not permitted
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.full_recoverseg')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list0, serial=True)

    def gpstate_resync_object_count(self):
        self.check_system()

        config = GPDBConfig()
        db_id = config.get_dbid(0,'p')

        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_before_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_transition_to_resync', 'suspend', 'primary']))
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_transition_to_resync_mark_recreate', 'suspend', 'primary']))
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml_ct')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg', [True]))

        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.check_fault_status', ['filerep_transition_to_resync'], {'seg_id': db_id}))

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpprimarymirror')

        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate_shell_cmd', ['-e']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.verify_gpstate_output')

        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.resume_faults', ['filerep_transition_to_resync', 'primary']))

        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.check_fault_status', ['filerep_transition_to_resync_mark_recreate'], {'seg_id': db_id}))

        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpprimarymirror')

        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate_shell_cmd', ['-e']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.verify_gpstate_output')
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.resume_faults', ['filerep_transition_to_resync_mark_recreate', 'primary']))
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        test_case_list0.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db')
        self.test_case_scenario.append(test_case_list0, serial=True)

    def mirror_resync_postmaster_reset_filerep_fault(self, filerep_fault):
        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        self.test_case_scenario.append(test_case_list1, serial=True)

        test_case_list3 = []
        test_case_list3.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_resync', 'suspend', 'primary']))
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', [filerep_fault, 'panic', 'mirror']))
        test_case_list5.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.resume_faults', ['filerep_resync', 'primary']))
        self.test_case_scenario.append(test_case_list5, serial=True)
        
        self.fts_test_run('primary','ct')


    def primary_resync_postmaster_reset_with_faults(self, filerep_fault, fault_type, filerep_role, gpstate_role, gprecover='no'):
        
        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        self.test_case_scenario.append(test_case_list1, serial=True)

        test_case_list2 = []
        test_case_list2.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_resync', 'suspend', 'primary']))
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate', [gpstate_role,'resync_incr']))
        self.test_case_scenario.append(test_case_list4) 
        if filerep_fault == 'filerep_consumer' and filerep_role == 'primary':
            test_case_list5_0 = []
            test_case_list5_0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', [filerep_fault, 'reset', filerep_role]))
            self.test_case_scenario.append(test_case_list5_0)

        test_case_list5 = []
        test_case_list5.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', [filerep_fault, fault_type, filerep_role]))
        self.test_case_scenario.append(test_case_list5)

        if filerep_fault == 'postmaster' and filerep_role == 'primary' :
            tinctest.logger.info('When its postmaster no need to resume filerep_resync')
        else:
            test_case_list5_2 = []
            test_case_list5_2.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.resume_faults', ['filerep_resync', 'primary']))
            self.test_case_scenario.append(test_case_list5_2)

        if filerep_fault == 'postmaster' and filerep_role == 'primary' :
            wait_for_db = False
        else:
            wait_for_db = True
    
        test_case_list6 = []
        test_case_list6.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql', [wait_for_db]))
        self.test_case_scenario.append(test_case_list6)

        if filerep_fault == 'postmaster' and filerep_role == 'primary' :
            test_case_list7_1 = []
            test_case_list7_1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db')
            test_case_list7_1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
            self.test_case_scenario.append(test_case_list7_1, serial=True)

        test_case_list7 = []
        test_case_list7.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_fts_test_ddl_dml')
        self.test_case_scenario.append(test_case_list7)

        test_case_list8 = []
        test_case_list8.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_gpstate', [gpstate_role,'ct']))
        self.test_case_scenario.append(test_case_list8) 

        if gprecover == 'yes':
            test_case_list9_1 = []
            test_case_list9_1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
            self.test_case_scenario.append(test_case_list9_1)

        test_case_list9 = []
        test_case_list9.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_insync')
        self.test_case_scenario.append(test_case_list9)

    def mirror_resync_postmaster_reset_with_faults(self):
             
        self.check_system()

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.run_trigger_sql')
        test_case_list1.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.wait_till_change_tracking')
        self.test_case_scenario.append(test_case_list1, serial=True)

        test_case_list2 = []
        test_case_list2.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_resync', 'suspend', 'primary']))
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.incremental_recoverseg')
        self.test_case_scenario.append(test_case_list3)
    
        test_case_list4 = []
        test_case_list4.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'panic', 'mirror']))
        test_case_list4.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.resume_faults', ['filerep_resync', 'primary']))
        self.test_case_scenario.append(test_case_list4, serial=True) 

        self.fts_test_run('primary', 'ct')



    def wait_for_shutdown_before_commit(self):
        self.check_system()

        config = GPDBConfig()
        db_id = config.get_dbid(-1,'p')

        test_case_list0 = []
        test_case_list0.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['fts_wait_for_shutdown', 'infinite_loop'], {'seg_id': db_id}))
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.set_faults', ['filerep_consumer', 'fault', 'primary']))
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append(('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.check_fault_status', ['fts_wait_for_shutdown'], {'seg_id': db_id}))
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.restart_db_with_no_rc_check')
        self.test_case_scenario.append(test_case_list3)
    
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.fts.fts_transitions.FtsTransitions.cluster_state')
        self.test_case_scenario.append(test_case_list4)
        

        
