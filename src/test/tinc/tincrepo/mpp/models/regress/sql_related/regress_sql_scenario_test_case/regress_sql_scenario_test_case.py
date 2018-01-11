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

from datetime import datetime
from time import sleep

import unittest2 as unittest
from tinctest.models.scenario import ScenarioTestCase
from mpp.models import SQLTestCase
from gppylib.commands.base import Command

t_prefix = 'mpp.models.regress.sql_related.regress_sql_scenario_test_case.regress_sql_scenario_test_case.'
@unittest.skip('mock')
class MockSQLScenaioTestCase(ScenarioTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def __init__(self, methodName):
        super(MockSQLScenaioTestCase, self).__init__(methodName)
        cmd = Command(name="dropdb", cmdStr='dropdb temp')
        cmd.run()
        from mpp.models.regress.sql_related.regress_sql_scenario_test_case.regress_sql_scenario_test_case import MockSQLTestCase as mtc
        mtc.__unittest_skip__ = False

    def test_serial_execution(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query02')
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query03')
        self.test_case_scenario.append(test_case_list, serial=True)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockSQLTestCase.test_query03')
        test_case_list2.append(t_prefix+'MockSQLTestCase.test_query03')
        self.test_case_scenario.append(test_case_list2)

    def test_fail_no_fastfail_execution(self):
        """
        @fail_fast False
        """
        test_case_list = []
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query02')
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query03')
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query04')
        self.test_case_scenario.append(test_case_list, serial=True)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockSQLTestCase.test_query03')
        test_case_list2.append(t_prefix+'MockSQLTestCase.test_query05')
        self.test_case_scenario.append(test_case_list2)

    def test_fail_fastfail_execution(self):
        """
        @fail_fast True
        """
        test_case_list = []
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query02')
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query03')
        test_case_list.append(t_prefix+'MockSQLTestCase.test_query04')
        self.test_case_scenario.append(test_case_list, serial=True)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockSQLTestCase.test_query03')
        test_case_list2.append(t_prefix+'MockSQLTestCase.test_query05')
        self.test_case_scenario.append(test_case_list2)

@unittest.skip('mock')
class MockSQLTestCase(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @db_name temp
    """
    def __init__(self, methodName):
        super(MockSQLTestCase, self).__init__(methodName)

class ScenarioTestCaseTests(unittest.TestCase):

    def test_construction(self):
        tinc_test_case = MockSQLScenaioTestCase('test_serial_execution')
        self.assertEqual(tinc_test_case.name, 'MockSQLScenaioTestCase.test_serial_execution')
        self.assertEqual(tinc_test_case.author, "balasr3")
        self.assertEqual(tinc_test_case.description, "test case with metadata")
        self.assertEqual(tinc_test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(len(tinc_test_case.tags), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 0)
        self.assertTrue(tinc_test_case.fail_fast)

    def test_sanity_run(self):
        tinc_test_case = MockSQLScenaioTestCase('test_serial_execution')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = unittest.TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 2)
        self.assertEqual(len(result.failures), 0)
        self.assertEqual(len(result.errors), 0)

    def test_fail_no_fastfail_execution(self):
        tinc_test_case = MockSQLScenaioTestCase('test_fail_no_fastfail_execution')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = unittest.TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 3)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 2)
        self.assertEqual(len(result.failures), 1)
        self.assertTrue('MockSQLTestCase.test_query04' in result.failures[0][1])
        self.assertTrue('MockSQLTestCase.test_query05' in result.failures[0][1])
        self.assertEqual(len(result.errors), 0)

    def test_fail_fastfail_execution(self):
        tinc_test_case = MockSQLScenaioTestCase('test_fail_fastfail_execution')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = unittest.TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 3)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 2)
        self.assertEqual(len(result.failures), 1)
        self.assertTrue('MockSQLTestCase.test_query04' in result.failures[0][1])
        self.assertFalse('MockSQLTestCase.test_query05' in result.failures[0][1])
        self.assertEqual(len(result.errors), 0)
