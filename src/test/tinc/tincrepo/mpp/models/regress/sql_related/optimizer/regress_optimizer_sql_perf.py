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

import os
import tinctest
from tinctest.lib import local_path

from mpp.models import OptimizerSQLPerformanceTestCase


import unittest2 as unittest

# we're testing SQLTestCase as it pertains to tinc.py (and only tinc.py)
# as such, any attempts by raw unit2 to discover and load MockSQLTestCase must be averted
@unittest.skip('mock')
class MockSQLPerformanceTestCase(OptimizerSQLPerformanceTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @drop_caches False
    @repetitions 2
    """
    def setUp(self):
        pass
    def test_00000(self):
        """
        @baseline 4.2.2.0
        @threshold 10
        """
        pass
    def tearDown(self):
        pass

class SQLPerformanceTestCaseTests(unittest.TestCase):


    def test_run_sql_test_success(self):
        test_case = MockSQLPerformanceTestCase('test_query02')

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False
        self.assertEqual(test_case.name, "MockSQLPerformanceTestCase.test_query02")
        self.assertEqual(test_case.author, 'kumara64')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))
        self.assertEqual(test_case.repetitions, 3)
        self.assertEqual(test_case.threshold, 10)
        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

    def test_run_all_perf_tests(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLPerformanceTestCase)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        for test_case in test_suite._tests:
            test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_suite.run(test_result)
        self.assertEqual(test_result.testsRun, 4)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
