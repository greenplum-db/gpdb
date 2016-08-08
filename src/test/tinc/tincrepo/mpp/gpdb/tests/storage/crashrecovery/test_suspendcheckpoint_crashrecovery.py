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
from tinctest.lib import local_path
from tinctest.models.scenario import ScenarioTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.filerep_util import Filerepe2e_Util


class SuspendcheckpointCrashrecoveryTestCase(ScenarioTestCase):
    ''' Testing state of prepared transactions upon crash-recovery
    @gucs gp_create_table_random_default_distribution=off
    '''

    def __init__(self, methodName):
        self.gpfile = Gpfilespace()
        self.filereputil = Filerepe2e_Util()
        super(SuspendcheckpointCrashrecoveryTestCase,self).__init__(methodName)
    
    def setUp(self):
        '''Create filespace '''
        self.gpfile.create_filespace('filespace_test_a')

    def tearDown(self):
        ''' Cleanup up the filespace created , reset skip chekpoint fault'''
        self.gpfile.drop_filespace('filespace_test_a')
        port = os.getenv('PGPORT')
        self.filereputil.inject_fault(f='checkpoint', y='reset', r='primary', o='0', p=port)
        


    def test_crash_recovery(self):
        ''' 
        @note : I kept the same name as cdbfast. This can be changed 
        @param skip_state : skip/noskip checkpoint
        @param cluster_state : sync/change_tracking/resync
        @param ddl_type : create/drop
        @fault_type : commit/abort . Uses the same parameter to pass in 'end_prepare_two_phase_sleep'
        @crash_type : gpstop_i/gpstop_a/failover_to_primary/failover_to_mirror
        @description: Test the state of prepared transactions upon crash-recovery. 
                      Faults are used to suspend the transactions before segments flush commit/abort to xlog. 
                      Different types of crash followed by recovery are performed to evaluate the transaction state
            Steps:
                0. Check the state of the cluster before proceeding the test execution
                1. Run any faults before pre_sqls depending on the clsuter_state 
                2. Run pre_sqls if any
                3. Run any faults required before the trigger_sqls based on the fault_type as well as cluster_state
                4. Run trigger_sqls - these are the transactions which will be suspended
                5. Crash and recover. Resume suspended faults if needed
                6. Run post_sqls to validate whether the transactions at step 4 are commited/ aborted as expected
                7. Recover and Validate using gpcheckcat and gpcheckmirrorseg

        @data_provider data_types_provider
        '''

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s " % self.test_data[0][1])
        tinctest.logger.info("\n ===============================================")
        pass_num = self.test_data[1][0]
        cluster_state = self.test_data[1][1]
        ddl_type = self.test_data[1][2]
        test_type = self.test_data[1][3]
        aborting_create_needed = self.test_data[1][4]
        
        if test_type == 'abort':
            test_dir = '%s_%s_tests' % ('abort', ddl_type)
        elif aborting_create_needed == 'True':
            test_dir = '%s_%s_%s_tests' % ('abort', ddl_type, 'needed')
        else:
            test_dir = '%s_%s_tests' % (test_type, ddl_type)
        if aborting_create_needed == True and test_type == 'commit':
            test_dir = 'abort_create_needed_tests'
        elif aborting_create_needed == True and test_type == 'abort' :
            test_dir = 'abort_abort_create_needed_tests'


        tinctest.logger.info("TestDir == %s " % test_dir )
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_system')
        self.test_case_scenario.append(test_case_list0)
        test_case_list1 = []
        test_case_list1.append(('storage.crashrecovery.SuspendCheckpointCrashRecovery.set_faults_before_executing_pre_sqls', [cluster_state]))
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('storage.crashrecovery.%s.pre_sql.test_pre_sqls.TestPreSQLClass' % test_dir)
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append(('storage.crashrecovery.SuspendCheckpointCrashRecovery.set_faults_before_executing_trigger_sqls', [pass_num, cluster_state, test_type, ddl_type, aborting_create_needed]))
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('storage.crashrecovery.%s.trigger_sql.test_triggersqls.TestTriggerSQLClass' % test_dir)
        test_case_list4.append(('storage.crashrecovery.SuspendCheckpointCrashRecovery.run_crash_and_recovery', [test_dir, cluster_state]))
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('storage.crashrecovery.%s.post_sql.test_postsqls.TestPostSQLClass' % test_dir)
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append(('storage.crashrecovery.SuspendCheckpointCrashRecovery.validate_system',[cluster_state]))
        self.test_case_scenario.append(test_case_list6)
        
@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {
#            '04_pass_num1_sync_create_commit_aborting_create_not_needed': [1,'sync','create','commit',False]
#            ,'05_pass_num2_sync_create_commit_aborting_create_not_needed': [2,'sync','create','commit',False]
#            ,'06_pass_num1_change_tracking_create_commit_aborting_create_not_needed': [1,'change_tracking','create','commit',False]
#            ,'07_commit_phase2_pass2_create_inchangetracking':[2,'change_tracking','create','commit',False]
#            ,'08_commit_phase2_pass1_create_inresync':[1,'resync','create','commit',False]
#            ,'09_commit_phase2_pass2_create_inresync':[2,'resync','create','commit',False]
#            ,'10_commit_phase2_pass1_drop_insync':[1,'sync','drop','commit',False]
#            ,'11_commit_phase2_pass2_drop_insync':[2,'sync','drop','commit',False]
#            ,'12_commit_phase2_pass1_drop_inchangetracking':[1,'change_tracking','drop','commit',False]
#            ,'13_commit_phase2_pass2_drop_inchangetracking':[2,'change_tracking','drop','commit',False]
#            ,'14_commit_phase2_pass1_drop_inresync':[1,'resync','drop','commit',False]
#            ,'15_commit_phase2_pass2_drop_inresync':[2,'resync','drop','commit',False]
#            ,'16_abort_phase1_create_insync':[0,'sync','create','abort',False]
#            ,'17_abort_phase1_create_inchangetracking':[0,'change_tracking','create','abort',False]
#            ,'18_abort_phase1_create_inresync':[0,'resync','create','abort',False]
#            ,'19_abort_phase2_pass1_create_insync':[1,'sync','create','abort',False]
#            ,'20_abort_phase2_pass2_create_insync':[2,'sync','create','abort',False]
#            ,'21_abort_phase2_pass1_create_inchangetracking':[1,'change_tracking','create','abort',False]
#            ,'22_abort_phase2_pass2_create_inchangetracking':[2,'change_tracking','create','abort',False]
#            ,'23_abort_phase2_pass1_create_inresync':[1,'resync','create','abort',False]
#            ,'24_abort_phase2_pass2_create_inresync':[2,'resync','create','abort',False]
#            ,'25_commit_phase1_aborting_create_needed_insync':[0,'sync','create','commit',True]
#            ,'26_commit_phase1_aborting_create_needed_inchangetracking':[0,'change_tracking','create','commit',True]
#            ,'27_commit_phase1_aborting_create_needed_inresync':[0,'resync','create','commit',True]
#            ,'28_commit_phase2_pass1_aborting_create_needed_insync':[1,'sync','create','commit',True]
#            ,'29_commit_phase2_pass2_aborting_create_needed_insync':[2,'sync','create','commit',True]
#            ,'30_commit_phase2_pass1_aborting_create_needed_inchangetracking':[1,'change_tracking','create','commit',True]
#            ,'31_commit_phase2_pass2_aborting_create_needed_inchangetracking':[2,'change_tracking','create','commit',True]
#            ,'32_commit_phase2_pass1_aborting_create_needed_inresync':[1,'resync','create','commit',True]
#            ,'33_commit_phase2_pass2_aborting_create_needed_inresync':[2,'resync','create','commit',True]
#            ,'34_abort_phase1_aborting_create_needed_insync':[0,'sync','create','abort',True]
#            ,'35_abort_phase1_aborting_create_needed_inchangetracking':[0,'change_tracking','create','abort',True]
#            ,'36_abort_phase1_aborting_create_needed_inresync':[0,'resync','create','abort',True]
#            ,'37_abort_phase2_pass1_aborting_create_needed_insync':[1,'sync','create','abort',True]
#            ,'38_abort_phase2_pass2_aborting_create_needed_insync':[2,'sync','create','abort',True]
#            ,'39_abort_phase2_pass1_aborting_create_needed_inchangetracking':[1,'change_tracking','create','abort',True]
#            ,'40_abort_phase2_pass2_aborting_create_needed_inchangetracking':[2,'change_tracking','create','abort',True]
#            ,'41_abort_phase2_pass1_aborting_create_needed_inresync':[1,'resync','create','abort',True]
            '42_abort_phase2_pass2_aborting_create_needed_inresync':[2,'resync','create','abort',True]
            }

 
  
    return data
        
