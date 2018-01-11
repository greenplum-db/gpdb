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

from tinctest.models.scenario import ScenarioTestCase

'''
Automation for MPP-18932
'''
class Mpp18932(ScenarioTestCase):

    """
    
    @description test cases for MPP-18932
    @created 2013-03-27 10:10:10
    @gucs gp_create_table_random_default_distribution=off
    @modified 2013-04-22 17:10:15
    @tags long_running  schedule_long-running 
    @product_version gpdb: [4.2.5.0- main]

    """
    @classmethod
    def setUpClass(cls):
        tinctest.logger.info('Running test for MPP-18816')
    
    def Dtest_scenario_setup(self):
        """
        Skipping this test to run on CI. This is very specific to the machine we are running. Since the test checks for OOD scenario
        """ 
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.filerep.mpp18932.setup.setup.Setup")
        self.test_case_scenario.append(test_case_list1)
        
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.filerep.mpp18932.steps.steps.TestSteps.test_change_tracking")
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.filerep.mpp18932.steps.steps.TestSteps.test_fillDisk")
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.filerep.mpp18932.runsql.sql.RunSql")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.filerep.mpp18932.steps.steps.TestSteps.test_checkLog_recover")
        self.test_case_scenario.append(test_case_list5)


		



