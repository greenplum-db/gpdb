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
from mpp.gpdb.tests.catalog.schema_topology import test_ST_GPFilespaceTablespaceTest
from tinctest.models.scenario import ScenarioTestCase

from tinctest.loader import TINCTestLoader
from mpp.gpdb.tests.storage.lib import Database

class CrashRecoverySchemaTopologyTestCase(ScenarioTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off;optimizer_print_missing_stats=off
    """

    def __init__(self, methodName):
        super(CrashRecoverySchemaTopologyTestCase, self).__init__(methodName)
        db = Database()
        db.setupDatabase('gptest')
        #Run setup class which creates database componets, filespaces etc.
        test_ST_GPFilespaceTablespaceTest.GPFilespaceTablespaceTest.setUpClass()

    def execute_individual_tests(self):
        '''
        This test runs four schema topology tests from classlist. Each of these four tests has a number of sql files.
        tloader.loadTestsFromName creates test methods for individual sql files.
        For each sql file in each test case we first run the suspend check point fault injector, next we run the test method gerenrated earlier
        finally we run checks.
        '''

        classlist = []
        classlist.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_OSSpecificSQLsTest.OSSpecificSQLsTest')
        classlist.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_AllSQLsTest.AllSQLsTest')

        for classname in classlist:

            tinctest.logger.info("\n\nrunning the test")
            tloader = TINCTestLoader()
            tests = tloader.loadTestsFromName(name=classname)
            tinctest.logger.info("\n\nstarting setup")

            test_list_2 = []
            test_list_2.append('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.run_fault_injector_to_skip_checkpoint')
            self.test_case_scenario.append(test_list_2)

            for test in tests:
                testname = '%s.%s.%s' %(test.__class__.__module__, test.__class__.__name__, test._testMethodName)
                tinctest.logger.info(testname)

                test_list_3 = []
                test_list_3.append(testname)
                self.test_case_scenario.append(test_list_3)
                tinctest.logger.info(testname)

            test_list_4 = []
            test_list_4.append('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.do_post_run_checks')
            self.test_case_scenario.append(test_list_4)

    def test_check_schema_topology(self):
        tinctest.logger.info("running check function...")
        self.execute_individual_tests()

