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

from mpp.gpdb.tests.storage.lib import Database
from tinctest.models.scenario import ScenarioTestCase

'''
Automation for MPP-18816
'''
class Mpp18816(ScenarioTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    
    @description Test MPP-18816, pgproc_race condition
    @created 2013-03-15 00:00:00
    @modified 2013-05-01 00:00:00
    @tags storage DTM schedule_long-running
    @product_version gpdb: [4.2.5.0- main]
    """

    @classmethod
    def setUpClass(cls):
        db = Database()
        db.setupDatabase(dbname ='mpp18816_db')

    def test_pgproc_race_condition(self):
        tinctest.logger.info('Running test for MPP-18816')
    
        #Run the create drop sqls concurrently
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.filerep.mpp18816.runsql.runsql.RunSql")
        self.test_case_scenario.append(test_case_list1)
        
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.filerep.mpp18816.verify.verify.Verification")
        self.test_case_scenario.append(test_case_list2)


