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

import unittest2 as unittest

from tinctest import TINCTestLoader

from mpp.models import SQLPerformanceTestCase

@unittest.skip('mock')
class MockSQLPerformanceTestCase(SQLPerformanceTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg performance
    @repetitions 3
    @threshold 10
    """
    def setUp(self):
        pass
    def test_explicit_definition(self):
        pass

class SQLPerformanceTestCaseTests(unittest.TestCase):
    def test_infer_metadata(self):
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLPerformanceTestCase)
        test_case = None
        for case in test_suite._tests:
            if case.name == "MockSQLPerformanceTestCase.test_query02":
                test_case = case
        self.assertNotEqual(test_case, None)
        self.assertEqual(test_case.name, "MockSQLPerformanceTestCase.test_query02")
        self.assertEqual(test_case.author, 'kumara64')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor', 'performance']))
        self.assertEqual(test_case.repetitions, 3)
        self.assertEqual(test_case.threshold, 10)

    def test_direct_instantiation(self):
        test_case = MockSQLPerformanceTestCase('test_query02')
        self.assertEqual(test_case.name, "MockSQLPerformanceTestCase.test_query02")
        self.assertEqual(test_case.author, 'kumara64')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor', 'performance']))
        self.assertEqual(test_case.repetitions, 3)
        self.assertEqual(test_case.threshold, 10)

    def test_explicit_test_fixtures(self):
        test_case = MockSQLPerformanceTestCase('test_explicit_definition')
        self.assertEqual(test_case.name, "MockSQLPerformanceTestCase.test_explicit_definition")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test case with metadata')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'performance']))
        self.assertEqual(test_case.repetitions, 3)
        self.assertEqual(test_case.threshold, 10)


