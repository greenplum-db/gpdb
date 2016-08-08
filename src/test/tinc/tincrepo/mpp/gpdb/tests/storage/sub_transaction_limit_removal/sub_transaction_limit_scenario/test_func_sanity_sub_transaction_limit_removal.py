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


    def test_sanity(self):
        self.run_sanity_test()
        
    def run_sanity_test(self):
        '''
        @tags sanity
        '''     
        
        path_to_init="mpp.gpdb.tests.storage.sub_transaction_limit_removal.sub_transaction_limit_scenario.SubTransactionLimitRemovalTestCase."

        cleandb = []
        cleandb.append(path_to_init+"cleandb")
        self.test_case_scenario.append(cleandb)

        check_system = []
        check_system.append(path_to_init+"check_system")
        self.test_case_scenario.append(check_system)

        run_sqls = []
        run_sqls.append((path_to_init+"run_sqls", ['failover_sql/subt_create_table_ao.sql']))
        self.test_case_scenario.append(run_sqls)
        
        run_post_sqls = []
        run_post_sqls.append((path_to_init+"run_post_sqls"))
        self.test_case_scenario.append(run_post_sqls)

