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
import mpp.gpdb.tests.storage.persistent

from mpp.gpdb.tests.storage.lib import Database
from tinctest.models.scenario import ScenarioTestCase

'''
Automation for MPP-11179
'''
class Mpp11179(ScenarioTestCase):
    """
    
    @description test cases for MPP-11179
    @created 2013-04-17 10:10:10
    @modified 2013-04-22 17:10:15
    @tags persistent tables schedule_persistent 
    @product_version gpdb: [4.1.3.x- main]
    """
    @classmethod
    def setUpClass(cls):
        db = Database()
        db.setupDatabase(dbname ='mpp11179_db')
    
    @classmethod
    def tearDownClass(cls):
        db = Database()
        db.dropDatabase(dbname = 'mpp11179_db' )

    def test_scenario_setup(self):
        tinctest.logger.info('Running test for MPP-11179')
        
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.persistent.mpp11179.steps.steps.Steps.test_runsql")
        
        test_case_list2.append("mpp.gpdb.tests.storage.persistent.mpp11179.steps.steps.Steps.test_checkpoint")

        test_case_list2.append("mpp.gpdb.tests.storage.persistent.mpp11179.steps.steps.Steps.test_gprestart")
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.persistent.mpp11179.steps.steps.Steps.test_gpcheckcat")
        self.test_case_scenario.append(test_case_list3)
        


		



