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

import os
import shutil

import unittest2 as unittest

import tinctest

from tinctest.lib import local_path

from mpp.gpdb.lib.models.sql.template import SQLTemplateTestCase

@unittest.skip('mock')
class MockSQLTemplateTestCase(SQLTemplateTestCase):
    pass

class MySQLTemplateTestCase(unittest.TestCase):
    """
    Test cases for sql template test case.
    """
    
    def test_user_defined_transform_files_exist(self):
        """
        Test whether user defined transformations are applied correctly to a sql template test case
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTemplateTestCase)
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTemplateTestCase.test_user_defined_transform":
                test_case = temp
        self.assertIsNotNone(test_case)
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        test_case.transforms['%sleep_interval%'] = "1"
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.failures), 0)
        self.assertEqual(len(test_result.errors), 0)
        self.assertTrue(os.path.exists(local_path("user_defined_transform.sql.t")))
        self.assertTrue(os.path.exists(local_path("user_defined_transform.ans.t")))
        os.remove(local_path("user_defined_transform.sql.t"))
        os.remove(local_path("user_defined_transform.ans.t"))

    def test_pre_defined_transform_files_exist(self):
        """
        Test whether the default transformations are applied correctly to a sql template case
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTemplateTestCase)
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTemplateTestCase.test_pre_defined_transform":
                test_case = temp
        self.assertIsNotNone(test_case)
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.failures), 0)
        self.assertEqual(len(test_result.errors), 0)
        self.assertTrue(os.path.exists(local_path("pre_defined_transform.sql.t")))
        self.assertTrue(os.path.exists(local_path("pre_defined_transform.ans.t")))
        schema = "%schema%"
        with open(local_path("pre_defined_transform.sql.t"), 'r') as input:
            for line in input.readlines():
                if schema in line:
                    self.assertTrue(False)
        with open(local_path("pre_defined_transform.ans.t"), 'r') as input:
            for line in input.readlines():
                if schema in line:
                    self.assertTrue(False)
        os.remove(local_path("pre_defined_transform.sql.t"))
        os.remove(local_path("pre_defined_transform.ans.t"))

