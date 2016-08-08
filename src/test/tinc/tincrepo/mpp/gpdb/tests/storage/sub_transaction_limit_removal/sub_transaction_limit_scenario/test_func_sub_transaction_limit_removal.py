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

import tinctest

from tinctest.lib import local_path
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.sub_transaction_limit_removal.sub_transaction_limit_scenario import SubTransactionLimitRemovalTestCase

class SubTransactionLimitRemovalScenarioTestCase(ScenarioTestCase):

    '''
    @summary: The basic idea of the test is to have a long script wiht many nested subtransactions running. 
              While it is running, we inject a subtrnasaction fault and then cause failover to primary, mirror or postmaster failure. 
              Depending on the failure, the transactions will either be committed or aborted. We chack this using post sqls.

              More details are found on : https://confluence.greenplum.com/display/QA/Sub+transaction+limit+removal
    '''

    def __init__(self, methodName):
        self.path = local_path("data")
        super(SubTransactionLimitRemovalScenarioTestCase,self).__init__(methodName)


    def test_sub_trans_sqls(self):
        self.run_sql_tests()

    def test_sub_trans_lim_removal_01(self):
        self.run_tests('SubtransactionFlushToFile', 'failover_to_mirror')
    def test_sub_trans_lim_removal_02(self):
        self.run_tests('SubtransactionReadFromFile', 'failover_to_mirror')
    def test_sub_trans_lim_removal_03(self):
        self.run_tests('SubtransactionRelease', 'failover_to_mirror')
    def test_sub_trans_lim_removal_04(self):
        self.run_tests('SubtransactionRollback', 'failover_to_mirror')

    def test_sub_trans_lim_removal_05(self):
        self.run_tests('SubtransactionFlushToFile', 'postmaster_reset')
    def test_sub_trans_lim_removal_06(self):
        self.run_tests('SubtransactionReadFromFile', 'postmaster_reset')
    def test_sub_trans_lim_removal_07(self):
        self.run_tests('SubtransactionRelease', 'postmaster_reset')
    def test_sub_trans_lim_removal_08(self):
        self.run_tests('SubtransactionRollback', 'postmaster_reset')

    def test_sub_trans_lim_removal_09(self):
        self.run_tests('SubtransactionReadFromFile', 'failover_to_primary')
    def test_sub_trans_lim_removal_10(self):
        self.run_tests('SubtransactionRelease', 'failover_to_primary')
    def test_sub_trans_lim_removal_11(self):
        self.run_tests('SubtransactionFlushToFile', 'failover_to_primary')
    def test_sub_trans_lim_removal_12(self):
        self.run_tests('SubtransactionRollback', 'failover_to_primary')


    def test_sub_trans_lim_removal_13(self):
        """
        @tags skip_ckpt
        """
        self.run_tests('SubtransactionFlushToFile', 'failover_to_mirror', True)
    def test_sub_trans_lim_removal_14(self):
        """
        @tags skip_ckpt
        """
        self.run_tests('SubtransactionReadFromFile', 'failover_to_mirror', True)
    def test_sub_trans_lim_removal_15(self):
        """
        @tags skip_ckpt
        """
        self.run_tests('SubtransactionRelease', 'failover_to_mirror', True)
    def test_sub_trans_lim_removal_16(self):
        """
        @tags skip_ckpt
        """
        self.run_tests('SubtransactionRollback', 'failover_to_mirror', True)

    def test_validate(self):
        self.run_validate()


    def run_sql_tests(self):
        path_to_init="mpp.gpdb.tests.storage.sub_transaction_limit_removal.sub_transaction_limit_scenario.SubTransactionLimitRemovalTestCase."
        path_to_sqls="mpp.gpdb.tests.storage.sub_transaction_limit_removal.sub_trans_sql_tests.sub_tans_sqls.SubTransTestCase"

        clean_files = []
        clean_files.append(path_to_init+"clean_files")
        self.test_case_scenario.append(clean_files)

        setup = []
        setup.append((path_to_init+"method_setup"))
        self.test_case_scenario.append(setup)

        st_sqls = []
        st_sqls.append(path_to_sqls)
        self.test_case_scenario.append(st_sqls,serial=True)

        clean_files = []
        clean_files.append(path_to_init+"clean_files")
        self.test_case_scenario.append(clean_files)


    def run_validate(self):
        path_to_init="mpp.gpdb.tests.storage.sub_transaction_limit_removal.sub_transaction_limit_scenario.SubTransactionLimitRemovalTestCase."
        validation = []
        validation.append((path_to_init+"_validation"))
        self.test_case_scenario.append(validation) 


    def run_tests(self, fault_name, trans_state, skip_ckpt=False):

        path_to_init="mpp.gpdb.tests.storage.sub_transaction_limit_removal.sub_transaction_limit_scenario.SubTransactionLimitRemovalTestCase."

        cleandb = []
        cleandb.append(path_to_init+"cleandb")
        self.test_case_scenario.append(cleandb) 
        
        check_system = []
        check_system.append(path_to_init+"check_system")
        self.test_case_scenario.append(check_system)
        
        if skip_ckpt:
            skip_checkpoint = []
            skip_checkpoint.append((path_to_init+"skip_checkpoint"))
            self.test_case_scenario.append(skip_checkpoint)

        suspend_faults = []
        suspend_faults.append((path_to_init+"suspend_faults", [fault_name]))
        self.test_case_scenario.append(suspend_faults)

        run_fault_and_sqls_in_parallel = []
        run_fault_and_sqls_in_parallel.append((path_to_init+"run_sqls", ['failover_sql/subt_create_table_ao.sql']))
        run_fault_and_sqls_in_parallel.append((path_to_init+"inject_and_resume_fault", [fault_name, trans_state]))
        self.test_case_scenario.append(run_fault_and_sqls_in_parallel)

        run_post_sqls = []
        run_post_sqls.append((path_to_init+"run_post_sqls", [fault_name, trans_state]))
        self.test_case_scenario.append(run_post_sqls) 

        run_restart_database = []
        run_restart_database.append((path_to_init+"run_restart_database"))
        self.test_case_scenario.append(run_restart_database)  

        run_gprecoverseg = []
        run_gprecoverseg.append((path_to_init+"run_gprecoverseg", ['incr']))
        self.test_case_scenario.append(run_gprecoverseg)  

        reset_all_faults = []
        reset_all_faults.append((path_to_init+"reset_all_faults"))
        self.test_case_scenario.append(reset_all_faults)

        if trans_state =='failover_to_mirror':
            run_restart_database = []
            run_restart_database.append((path_to_init+"run_restart_database"))
            self.test_case_scenario.append(run_restart_database)  

        kill_zombies = []
        kill_zombies.append(path_to_init+"kill_zombies")
        self.test_case_scenario.append(kill_zombies)
