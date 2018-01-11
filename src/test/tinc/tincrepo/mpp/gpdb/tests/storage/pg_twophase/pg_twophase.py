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
from mpp.lib.PSQL import PSQL
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.config import GPDBConfig
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.gpdb.tests.storage.lib.common_utils import copy_files_to_segments, copy_files_to_master
from tinctest.models.scenario import ScenarioTestCase
from mpp.models import MPPTestCase


class PgtwoPhaseTestCase(ScenarioTestCase, MPPTestCase):
    ''' Testing state of prepared transactions upon crash-recovery'''

    def __init__(self, methodName):
        self.gpfile = Gpfilespace()
        self.filereputil = Filerepe2e_Util()
        super(PgtwoPhaseTestCase,self).__init__(methodName)
    
    def setUp(self):
        '''Create filespace '''
        self.gpfile.create_filespace('filespace_test_a')
        super(PgtwoPhaseTestCase,self).setUp()
        
    def tearDown(self):
        ''' Cleanup up the filespace created , reset skip chekpoint fault'''
        self.gpfile.drop_filespace('filespace_test_a')
        port = os.getenv('PGPORT')
        self.filereputil.inject_fault(f='checkpoint', y='reset', r='primary', o='0', p=port)
        super(PgtwoPhaseTestCase,self).tearDown()

    def execute_split_sqls(self, skip_state, cluster_state, ddl_type, fault_type, crash_type):
        ''' 
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
                7. Recover the cluster in case if neeed
                8. Validate using gpcheckcat and gpcheckmirrorseg

        '''
        if fault_type == 'end_prepare_two_phase_sleep':
            test_dir = '%s_%s_tests' % ('abort', ddl_type)
        else:
            test_dir = '%s_%s_tests' % (fault_type, ddl_type)
        
        tinctest.logger.info('fault_type %s test_dir %s ddl_type %s' % (fault_type, test_dir, ddl_type))
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_system')
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.run_faults_before_pre', [cluster_state]))
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.pg_twophase.%s.pre_sql.test_presqls.TestPreSQLClass' % test_dir)
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.run_faults_before_trigger', [skip_state, cluster_state, fault_type]))
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.pg_twophase.%s.trigger_sql.test_triggersqls.TestTriggerSQLClass' % test_dir)
        test_case_list4.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.run_crash_and_recover', [crash_type, fault_type, test_dir, cluster_state, skip_state]))
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.storage.pg_twophase.%s.post_sql.test_postsqls.TestPostSQLClass' % test_dir)
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append(('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.run_gprecover', [crash_type, cluster_state]))
        self.test_case_scenario.append(test_case_list6)

        test_case_list7 = []
        test_case_list7.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.run_validation')
        self.test_case_scenario.append(test_case_list7)
        
        test_case_list8 = []
        test_case_list8.append('mpp.gpdb.tests.storage.pg_twophase.PgtwoPhaseClass.cleanup_dangling_processes')
        self.test_case_scenario.append(test_case_list8)
        
