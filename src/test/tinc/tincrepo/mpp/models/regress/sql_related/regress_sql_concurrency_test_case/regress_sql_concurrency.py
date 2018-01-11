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

import fnmatch
import inspect
import os
import shutil

from mpp.models import SQLConcurrencyTestCase
from tinctest.lib.system import TINCSystem
from tinctest.lib import local_path

import unittest2 as unittest

# we're testing SQLTestCase as it pertains to tinc.py (and only tinc.py)
# as such, any attempts by raw unit2 to discover and load MockSQLTestCase must be averted
@unittest.skip('mock')
class MockSQLConcurrencyTestCase(SQLConcurrencyTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def test_00000(self):
        """
        @baseline 4.2.2.0
        @threshold 10
        """
        pass

@unittest.skip('mock')
class MockSQLConcurrencyTestCaseTemplate(SQLConcurrencyTestCase):
    template_dir = 'template_dir'
    template_subs = {'%VAR%' : "foobar"}

class SQLConcurrencyTestCaseTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        """
        Some unit tests here call's test_case.run() method which does not set up the out dir required (out_dir set up gets
        handled in MPPTestCase.setUpClass which will be invoked only when running through a test_suite.run().
        Therefore, we setup the out dir required here in the test itself. 
        """
        TINCSystem.make_dirs(local_path('output'), ignore_exists_error=True)

    def test_run_sql_test_success(self):
        test_case = MockSQLConcurrencyTestCase('test_query02')

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLConcurrencyTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        i = 1
        for file in os.listdir(test_case.get_sql_dir()):
            if fnmatch.fnmatch(file, 'query02_part*.sql'):
                i += 1
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        count = 0
        for file in os.listdir(test_case.get_out_dir()):
            if fnmatch.fnmatch(file, 'query02*.out'):
                count = count + 1
        self.assertEqual(count, test_case.concurrency * test_case.iterations * i)
        # Cleanup
        if os.path.exists(test_case.get_out_dir()):
            for file in os.listdir(test_case.get_out_dir()):
                if fnmatch.fnmatch(file, 'query02*.out'):
                    os.remove(os.path.join(test_case.get_out_dir(), file))

    def test_run_sql_test_failure_with_gpdiff(self):
        test_case = MockSQLConcurrencyTestCase('test_query03')

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLConcurrencyTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        current_dir = os.path.dirname(inspect.getfile(test_case.__class__))
        i = 1
        for file in os.listdir(test_case.get_sql_dir()):
            if fnmatch.fnmatch(file, 'query03_part*.sql'):
                i += 1
        self.assertTrue(test_case.gpdiff)
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 1)
        # Cleanup
        if os.path.exists(test_case.get_out_dir()):
            for file in os.listdir(test_case.get_out_dir()):
                if fnmatch.fnmatch(file, 'query03*.out'):
                    os.remove(os.path.join(test_case.get_out_dir(),file))

    def test_run_sql_test_template(self):
        test_case = MockSQLConcurrencyTestCaseTemplate('test_template_query02')
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()

        # Cleanup
        if os.path.exists(test_case.get_out_dir()):
            for file in os.listdir(test_case.get_out_dir()):
                if fnmatch.fnmatch(file, 'template_query02**.out'):
                    os.remove(os.path.join(test_case.get_out_dir(),file))

        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        count = 0
        for file in os.listdir(test_case.get_out_dir()):
            if fnmatch.fnmatch(file, 'template_query02*.out'):
                count = count + 1
        self.assertEqual(count, 12)

        # Cleanup
        if os.path.exists(test_case.get_out_dir()):
            for file in os.listdir(test_case.get_out_dir()):
                path = os.path.join(test_case.get_out_dir(), file)
                if fnmatch.fnmatch(file, 'template_query02*.*'):
                    os.remove(os.path.join(test_case.get_out_dir(), file))
                if fnmatch.fnmatch(file, 'regress_sql_concurrency_test_case'):
                    shutil.rmtree(path)

    def test_explicit_test_method(self):
        test_case = MockSQLConcurrencyTestCase('test_00000')
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

        
