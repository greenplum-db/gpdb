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

import unittest2 as unittest

from datetime import datetime
from tinctest.models.perf import PerformanceTestCase


@unittest.skip('mock')
class MockPerformanceTestCase(PerformanceTestCase):
    """ Mock TINCTestCase used for below unittest TestCases """
    def test_001_do_stuff(self):
        self.assertTrue(True)


class MockPerformanceTestCaseWithMetadata(PerformanceTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags pdw
    @product_version gpdb: main
    @repetitions 3
    @baseline 4.2.x
    @threshold 5
    """
    def __init__(self, methodName = 'runTest'):
        self.count = 0
        super(MockPerformanceTestCaseWithMetadata, self).__init__(methodName)
    def setUp(self):
        pass
    def test_001_do_stuff(self):
        self.count += 1
        self.assertTrue(True)

class PerformanceTestCaseTests(unittest.TestCase):
    def test_001_sanity_construction(self):
        tinc_test_case = MockPerformanceTestCase('test_001_do_stuff')
        self.assertEqual(tinc_test_case.name, 'MockPerformanceTestCase.test_001_do_stuff')
        self.assertEqual(tinc_test_case.author, None)
        self.assertEqual(tinc_test_case.description, '')
        self.assertEqual(tinc_test_case.created_datetime,  datetime.strptime('2000-01-01 00:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2000-01-01 00:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(len(tinc_test_case.tags), 0)
        self.assertEqual(tinc_test_case.repetitions, 1)
        self.assertEqual(tinc_test_case.baseline, None)
        self.assertEqual(tinc_test_case.threshold, 5)

    def test_002_infer_metadata(self):
        test_case = MockPerformanceTestCaseWithMetadata('test_001_do_stuff')
        self.assertEqual(test_case.name, "MockPerformanceTestCaseWithMetadata.test_001_do_stuff")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test case with metadata')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['pdw']))
        self.assertEqual(test_case.repetitions, 3)
        self.assertEqual(test_case.baseline, '4.2.x')
        self.assertEqual(test_case.threshold, 5)
    
    def test_003_sanity_run_performance_test(self):
        test_case = MockPerformanceTestCaseWithMetadata('test_001_do_stuff')
        test_case.run()
        self.assertEqual(test_case.name, "MockPerformanceTestCaseWithMetadata.test_001_do_stuff")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test case with metadata')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['pdw']))
        self.assertEqual(test_case.repetitions, 3)
        self.assertEqual(test_case.baseline, '4.2.x')
        self.assertEqual(test_case.threshold, 5)
        self.assertEqual(test_case.count, 3)
        
