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
from tinctest.lib import local_path
from tinctest.models.scenario import ScenarioTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.filerep_util import Filerepe2e_Util


class SuspendcheckpointCrashrecoveryTestCase(ScenarioTestCase):
    '''
    Testing state of prepared transactions upon crash-recovery
    @gucs gp_create_table_random_default_distribution=off;optimizer_print_missing_stats=off
    '''

    def __init__(self, methodName):
        tinctest.logger.info("\n =====================In __init__ method ==========================")
        self.gpfile = Gpfilespace()
        self.filereputil = Filerepe2e_Util()
        super(SuspendcheckpointCrashrecoveryTestCase,self).__init__(methodName)
    
    def setUp(self):
        tinctest.logger.info("\n =====================In setUp method ==========================") 
        super(SuspendcheckpointCrashrecoveryTestCase, self).setUp()
        '''Create filespace '''
        self.gpfile.create_filespace('filespace_test_a')

    def tearDown(self):
        tinctest.logger.info("\n ======================In tesrDoen method =========================")
        ''' Cleanup up the filespace created , reset skip chekpoint fault'''
        self.gpfile.drop_filespace('filespace_test_a')
        port = os.getenv('PGPORT')
        self.filereputil.inject_fault(f='checkpoint', y='reset', r='primary', o='0', p=port)
        super(SuspendcheckpointCrashrecoveryTestCase, self).tearDown()
        


    def test_crash_recovery_31_to_42(self):
        ''' 
        @note : Steps are same as Cdbfast and Previous tinc schedule
        @param skip_state : skip checkpoint
        @param cluster_state : sync/change_tracking/resync
        @param ddl_type : create/drop
        @fault_type : commit/abort . 
        @crash_type : gpstop_i/gpstop_a/failover_to_primary
        @description: Test the state of prepared transactions upon crash-recovery.
                      Faults are used to suspend the transactions before segments flush commit/abort to xlog.
                      Crash followed by recovery are performed to evaluate the transaction state
            Steps:
                0. Check the state of the cluster before proceeding the test execution
                1. Run any fault 'skip checkpoint' before pre_sqls 
                2. Run pre_sqls if any
                3. Run any faults required before the trigger_sqls based on the fault_type as well as cluster_state
                4. Run trigger_sqls - these are the transactions which will be suspended
                5. Crash and recover. 
                6. Run post_sqls to validate whether the transactions at step 4 are commited/ aborted as expected
                7. Recover and Validate using gpcheckcat and gpcheckmirrorseg

        @data_provider data_types_provider
        '''

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s%s " % (self.test_data[0][0],self.test_data[0][1]))
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
        test_case_list1.append(('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.set_faults_before_executing_pre_sqls', [cluster_state]))
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.crashrecovery.%s.pre_sql.test_pre_sqls.TestPreSQLClass' % test_dir)
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append(('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.set_faults_before_executing_trigger_sqls', [pass_num, cluster_state, test_type, ddl_type, aborting_create_needed]))
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.crashrecovery.%s.trigger_sql.test_triggersqls.TestTriggerSQLClass' % test_dir)
        test_case_list4.append(('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.run_crash_and_recovery_fast', [test_dir, pass_num, cluster_state, test_type, ddl_type, aborting_create_needed]))
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.storage.crashrecovery.%s.post_sql.test_postsqls.TestPostSQLClass' % test_dir)
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append(('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.validate_system',[cluster_state]))
        self.test_case_scenario.append(test_case_list6)
        
@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {
            '31_commit_phase2_pass2_aborting_create_needed_inchangetracking':[2,'change_tracking','create','commit',True]
            ,'32_commit_phase2_pass1_aborting_create_needed_inresync':[1,'resync','create','commit',True]
            ,'33_commit_phase2_pass2_aborting_create_needed_inresync':[2,'resync','create','commit',True]
            ,'34_abort_phase1_aborting_create_needed_insync':[0,'sync','create','abort',True]
            ,'35_abort_phase1_aborting_create_needed_inchangetracking':[0,'change_tracking','create','abort',True]
            ,'36_abort_phase1_aborting_create_needed_inresync':[0,'resync','create','abort',True]
            ,'37_abort_phase2_pass1_aborting_create_needed_insync':[1,'sync','create','abort',True]
            ,'38_abort_phase2_pass2_aborting_create_needed_insync':[2,'sync','create','abort',True]
            ,'39_abort_phase2_pass1_aborting_create_needed_inchangetracking':[1,'change_tracking','create','abort',True]
            ,'40_abort_phase2_pass2_aborting_create_needed_inchangetracking':[2,'change_tracking','create','abort',True]
            ,'41_abort_phase2_pass1_aborting_create_needed_inresync':[1,'resync','create','abort',True]
            ,'42_abort_phase2_pass2_aborting_create_needed_inresync':[2,'resync','create','abort',True]
            }

 
  
    return data
        
