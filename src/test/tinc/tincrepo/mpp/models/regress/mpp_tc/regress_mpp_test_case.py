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

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import unittest2 as unittest
import tinctest
import os

from unittest2.runner import _WritelnDecorator
from contextlib import closing
from StringIO import StringIO

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL

@unittest.skip('mock')
class MockMPPTestCaseWithClassVariable(MPPTestCase):
    db_name = 'dataset_0.01_heap'
    def test_do_stuff(self):
        self.assertTrue(True)
    
@unittest.skip('mock')
class MockMPPTestCase(MPPTestCase):
    """
    @db_name dataset_0.01_heap
    """
    def test_do_stuff(self):
        self.assertTrue(True)

    def test_failure(self):
        self.assertTrue(False)

    def test_gather_logs(self):
        """
        @gather_logs_on_failure True
        @restart_on_failure True
        """
        PSQL.run_sql_command("select pg_sleep(5)")
        self.assertTrue(False)

    def test_restart_on_failure(self):
        """
        @gather_logs_on_failure True
        @restart_on_failure True
        """
        PSQL.run_sql_command("select * from some_table_that_does_not_exist_to_generate_log_errors")
        self.assertTrue(False)

class MPPTestCaseRegressionTests(unittest.TestCase):

    def test_sanity_run(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.regress.mpp_tc.regress_mpp_test_case.MockMPPTestCase.test_do_stuff')
        self.assertIsNotNone(test_suite)

        self.assertTrue(len(test_suite._tests), 1)

        for test in test_suite._tests:
            test.__class__.__unittest_skip__ = False

        with closing(_WritelnDecorator(StringIO())) as buffer:
            test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            test_suite.run(test_result)
            self.assertEqual(test_result.testsRun, 1)
            self.assertEqual(len(test_result.errors), 0)
            self.assertEqual(len(test_result.skipped), 0)
            self.assertEqual(len(test_result.failures), 0)

    def test_sanity_failure(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.regress.mpp_tc.regress_mpp_test_case.MockMPPTestCase.test_failure')
        self.assertIsNotNone(test_suite)

        self.assertTrue(len(test_suite._tests), 1)

        for test in test_suite._tests:
            test.__class__.__unittest_skip__ = False

        with closing(_WritelnDecorator(StringIO())) as buffer:
            test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            test_suite.run(test_result)
            self.assertEqual(test_result.testsRun, 1)
            self.assertEqual(len(test_result.errors), 0)
            self.assertEqual(len(test_result.skipped), 0)
            self.assertEqual(len(test_result.failures), 1)

    def test_failure_gather_logs(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.regress.mpp_tc.regress_mpp_test_case.MockMPPTestCase.test_gather_logs')
        self.assertIsNotNone(test_suite)

        self.assertTrue(len(test_suite._tests), 1)

        for test in test_suite._tests:
            test.__class__.__unittest_skip__ = False

        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = tinctest.TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(test_suite)
            self.assertEqual(test_result.testsRun, 1)
            self.assertEqual(len(test_result.errors), 0)
            self.assertEqual(len(test_result.skipped), 0)
            self.assertEqual(len(test_result.failures), 1)

        expected_log_file = os.path.join(MockMPPTestCase.get_out_dir(), test._testMethodName + '.logs')
        self.assertTrue(os.path.exists(expected_log_file))
        self.assertTrue(os.path.getsize(expected_log_file) > 0)

    def test_restart_on_failure(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.regress.mpp_tc.regress_mpp_test_case.MockMPPTestCase.test_restart_on_failure')
        self.assertIsNotNone(test_suite)

        self.assertTrue(len(test_suite._tests), 1)

        for test in test_suite._tests:
            test.__class__.__unittest_skip__ = False

        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = tinctest.TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(test_suite)
            self.assertEqual(test_result.testsRun, 1)
            self.assertEqual(len(test_result.errors), 0)
            self.assertEqual(len(test_result.skipped), 0)
            self.assertEqual(len(test_result.failures), 1)

        # TODO may be add a check later on to see if we actually restart the cluster. 
        expected_log_file = os.path.join(MockMPPTestCase.get_out_dir(), test._testMethodName + '.logs')
        self.assertTrue(os.path.exists(expected_log_file))
        self.assertTrue(os.path.getsize(expected_log_file) > 0)
        
class MPPTestCaseTestSetUpClass(unittest.TestCase):
    def test_setup(self):
        test_case = MockMPPTestCase('test_do_stuff')
        self.assertEqual(test_case.db_name, "dataset_0.01_heap")

class MPPTestCaseTestSetUpClassWithClassVariable(unittest.TestCase):
    def test_setup(self):
        test_case = MockMPPTestCaseWithClassVariable('test_do_stuff')
        self.assertEqual(test_case.db_name, "dataset_0.01_heap")
