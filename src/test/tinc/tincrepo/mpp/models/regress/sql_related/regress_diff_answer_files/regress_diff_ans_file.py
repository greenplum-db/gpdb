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
import tinctest
from mpp.models import SQLTestCase

@unittest.skip('mock')
class MockTestDifferentAnswerFilesWithOptMode(SQLTestCase):
    """
    @optimizer_mode both
    """
    sql_dir = '.'
    ans_dir = '.'
    out_dir = 'output/'

@unittest.skip('mock')
class MockTestDifferentAnswerFilesWithoutOptMode(SQLTestCase):
    """
    @optimizer_mode both
    """
    sql_dir = '.'
    ans_dir = '.'
    out_dir = 'output/'

class TestDifferentAnserFiles(unittest.TestCase):
    def test_run_all_with_opt_mode(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockTestDifferentAnswerFilesWithOptMode)
        for tc in test_suite._tests:
            tc.__class__.__unittest_skip__ = False
            test_result = unittest.TestResult()
            tc.run(test_result)
            self.assertEqual(len(test_result.errors), 0, 'test case failed: %s' %tc)
            self.assertEqual(len(test_result.failures), 0, 'test case failed: %s' %tc)

    def test_run_all_without_opt_mode(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockTestDifferentAnswerFilesWithoutOptMode)
        for tc in test_suite._tests:
            tc.__class__.__unittest_skip__ = False
            test_result = unittest.TestResult()
            tc.run(test_result)
            self.assertEqual(len(test_result.errors), 0, 'test case failed: %s' %tc)
            self.assertEqual(len(test_result.failures), 0, 'test case failed: %s' %tc)
