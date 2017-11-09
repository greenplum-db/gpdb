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
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.lib.gpfilespace import Gpfilespace
from tinctest.models.scenario import ScenarioTestCase


class SwitchCheckpointTestCase(ScenarioTestCase):
    ''' Testing state of transactions with faults on master after crash-recovery'''

    def __init__(self, methodName):
        self.gpfile = Gpfilespace()
        super(SwitchCheckpointTestCase,self).__init__(methodName)
    
    def setUp(self):
        '''Create filespace '''
        self.gpfile.create_filespace('filespace_test_a')
        super(SwitchCheckpointTestCase,self).setUp()

    def tearDown(self):
        ''' Cleanup up the filespace created '''
        self.gpfile.drop_filespace('filespace_test_a')
        super(SwitchCheckpointTestCase,self).tearDown()

    def switch_checkpoint(self, cluster_state, fault_type, crash_type):
        ''' 
        @param skip_state : skip/noskip checkpoint
        @param cluster_state : sync/change_tracking/resync
        @param ddl_type : create/drop
        @fault_type : commit/abort . Uses the same parameter to pass in 'end_prepare_two_phase_sleep'
        @crash_type : gpstop_i/gpstop_a/failover_to_primary/failover_to_mirror
        @description: Test the state of transactions after fault on master at diff stages followed by  crash-recovery. 
                      Faults are used to suspend the transactions at three stages
                      1. After prepare has been sent by master and acknowledge by all workers
                      2. After distributed commit has been flushed to xlog on master
                      3. After commit prepared has been sent out acknowledged before distributed forget
            Steps:
                0. Check the state of the cluster before proceeding the test execution
                1. Run pre_sqls
                2. Run any faults required before the trigger_sqls based on the fault_type as well as cluster_state
                3. Run switch_checkpoint loop. switch_xlog, checkpoint in case of 'dtm_broadcast_prepare' fault 
                4. Crash and recover. Resume suspended faults if needed
                5. Run validate_loop
                7. Recover the cluster in case if needed
                8. Validate using gpcheckcat and gpcheckmirrorseg

        '''
        test_dir = {"dtm_broadcast_prepare":"switch_ckpt_a,switch_ckpt_b", "dtm_broadcast_commit_prepared":"switch_ckpt_a,switch_ckpt_b"}

        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_system')
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        if fault_type == 'dtm_broadcast_commit_prepared':
            test_case_list1.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_a.pre_sql.test_presqls.TestPreSQLClass')
            test_case_list1.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.pre_sql.test_presqls.TestPreSQLClass')
        
        if fault_type == 'dtm_broadcast_prepare':
            test_case_list1.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_a.pre_sql.test_presqls.TestPreSQLClass')
            test_case_list1.append('mpp.gpdb.tests.storage.pg_twophase.checkpoint.test_checkpoint.TestCheckpointClass')
            test_case_list1.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.pre_sql.test_presqls.TestPreSQLClass')
         
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.switch_ckpt_faults_before_trigger', [cluster_state, fault_type]))
        self.test_case_scenario.append(test_case_list2)
        
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.switch_ckpt_switch_xlog')
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        if fault_type == 'dtm_broadcast_commit_prepared':
            test_case_list4.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_a.trigger_sql.test_triggersqls.TestTriggerSQLClass')
            test_case_list4.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.trigger_sql.test_triggersqls.TestTriggerSQLClass')
        
        if fault_type == 'dtm_broadcast_prepare':
            test_case_list4.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_a.trigger_sql.test_triggersqls.TestTriggerSQLClass')
            test_case_list4.append('mpp.gpdb.tests.storage.pg_twophase.checkpoint.test_checkpoint.TestCheckpointClass')
            test_case_list4.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.trigger_sql.test_triggersqls.TestTriggerSQLClass')
         
        test_case_list4.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.switch_ckpt_crash_and_recover', [crash_type, fault_type, test_dir[fault_type], cluster_state]))
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.run_gprecover', [crash_type, cluster_state]))
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        if fault_type == 'dtm_broadcast_commit_prepared':
            test_case_list6.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_a.post_sql.test_postsqls.TestPostSQLClass')
            test_case_list6.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.post_sql.test_postsqls.TestPostSQLClass')

        # No tables will be created for crash_type failover_to_mirror
        if fault_type == 'dtm_broadcast_prepare' and crash_type not in ('gpstop_i'):
            test_case_list6.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_a.post_sql.test_postsqls.TestPostSQLClass')
            test_case_list6.append('mpp.gpdb.tests.storage.pg_twophase.checkpoint.test_checkpoint.TestCheckpointClass')
            test_case_list6.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.post_sql.test_postsqls.TestPostSQLClass')
         
        self.test_case_scenario.append(test_case_list6)

        test_case_list7 = []
        if fault_type in ('dtm_broadcast_commit_prepared', 'dtm_broadcast_prepare'):
            test_case_list7.append('mpp.gpdb.tests.storage.pg_twophase.switch_ckpt_b.cleanup_sql.test_cleanup.TestCleanupClass')

        self.test_case_scenario.append(test_case_list7)

        test_case_list8 = []
        test_case_list8.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.run_validation')
        self.test_case_scenario.append(test_case_list8)

        test_case_list9 = []
        test_case_list9.append('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.cleanup_dangling_processes')
        self.test_case_scenario.append(test_case_list9)
