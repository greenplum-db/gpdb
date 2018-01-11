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

import unittest2 as unittest
from unittest2.runner import _WritelnDecorator

from tinctest import TINCTestLoader
from tinctest import TINCTextTestResult

from mpp.models import SQLConcurrencyTestCase

@unittest.skip('mock')
class MockSQLConcurrencyTestCase(SQLConcurrencyTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def setUp(self):
        pass
    def test_explicit_definition(self):
        pass
    def test_with_gpdiff(self):
        """
        @gpdiff True
        """
        pass

class SQLConcurrencyTestCaseTests(unittest.TestCase):
    def test_infer_metadata(self):
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLConcurrencyTestCase)
        test_case = None
        for case in test_suite._tests:
            if case.name == "MockSQLConcurrencyTestCase.test_query02":
                test_case = case
        self.assertNotEqual(test_case, None)
        self.assertEqual(test_case.name, "MockSQLConcurrencyTestCase.test_query02")
        self.assertEqual(test_case.author, 'kumara64')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))
        self.assertEqual(test_case.gpdiff, False)

    def test_with_gpdiff(self):
        test_case = MockSQLConcurrencyTestCase('test_with_gpdiff')
        self.assertEqual(test_case.gpdiff, True)
