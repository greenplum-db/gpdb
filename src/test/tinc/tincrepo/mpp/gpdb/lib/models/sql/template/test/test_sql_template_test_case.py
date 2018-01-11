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

from mpp.gpdb.lib.models.sql.template import SQLTemplateTestCase

import unittest2 as unittest

import shutil

# we're testing SQLTestCase as it pertains to tinc.py (and only tinc.py)
# as such, any attempts by raw unit2 to discover and load MockSQLTestCase must be averted
@unittest.skip('mock')
class MockSQLTemplateTestCase(SQLTemplateTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def setUp(self):
        pass

    def test_pre_defined_transform(self):
        pass

    def test_user_defined_transform(self):
        pass
 
class SQLTemplateTestCaseTests(unittest.TestCase):
    def test_pre_defined_transform_metadata(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTemplateTestCase)
        test_case = None
        for case in test_suite._tests:
            if case.name == "MockSQLTemplateTestCase.test_pre_defined_transform":
                test_case = case
        self.assertIsNotNone(test_case)
        self.assertEqual(test_case.name, "MockSQLTemplateTestCase.test_pre_defined_transform")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test case with metadata')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca','hashagg']))

    def test_user_defined_transform_metadata(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTemplateTestCase)
        test_case = None
        for case in test_suite._tests:
            if case.name == "MockSQLTemplateTestCase.test_user_defined_transform":
                test_case = case
        self.assertIsNotNone(test_case)
        self.assertEqual(test_case.name, "MockSQLTemplateTestCase.test_user_defined_transform")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test case with metadata')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca','hashagg']))
        
