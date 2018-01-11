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
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path
from tinctest.models.scenario import ScenarioTestCase

class DuplicateEntriesScenarioTestCase(ScenarioTestCase):
    """ 
    Automate the verification of MPP-19038 where duplicate entries were reporting but only 1 in table
    @gucs gp_create_table_random_default_distribution=off
    """

    def __init__(self, methodName):
        super(DuplicateEntriesScenarioTestCase,self).__init__(methodName)

    @classmethod
    def setUpClass(cls):
        tinctest.logger.info('Creating the tables & Initial setup')
        PSQL.run_sql_file(sql_file=local_path('create_tables.sql'),
                          out_file=local_path('create_tables.out'))     
    @classmethod
    def tearDownClass(cls):
        pass


    def test_mpp19038(self):
        test_case_list0 = []
        test_case_list0.append("mpp.gpdb.tests.storage.access_methods.duplicate_entries.DuplicateEntriesTestCase.killProcess_postmaster") 
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.access_methods.duplicate_entries.DuplicateEntriesTestCase.reindex_vacuum")
        self.test_case_scenario.append(test_case_list1)       

        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.access_methods.duplicate_entries.DuplicateEntriesTestCase.run_recover_rebalance")
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.access_methods.duplicate_entries.DuplicateEntriesTestCase.check_duplicate_entry")
        self.test_case_scenario.append(test_case_list3)
