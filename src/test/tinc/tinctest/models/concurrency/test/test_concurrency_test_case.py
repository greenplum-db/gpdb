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
from unittest2.runner import _WritelnDecorator

from datetime import datetime
from StringIO import StringIO
from time import sleep

import unittest2 as unittest

import tinctest
from tinctest.runner import TINCTextTestResult, TINCTestRunner
from tinctest.models.concurrency import ConcurrencyTestCase

@unittest.skip('mock')
class MockConcurrencyTestCase(ConcurrencyTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @concurrency 2
    @iterations 1
    """
    def __init__(self, methodName):
        super(MockConcurrencyTestCase, self).__init__(methodName)

    def setUp(self):
        pass
    def test_explicit_definition(self):
        sleep(1)
    def test_multiple_iterations(self):
        """
        @iterations 2
        """
        sleep(1)
    def test_failure(self):
        self.fail('Just like that ! ')


class TestConcurrencyTestCase(unittest.TestCase):
    def test_sanity_construction(self):
        tinc_test_case = MockConcurrencyTestCase('test_explicit_definition')
        self.assertEqual(tinc_test_case.name, 'MockConcurrencyTestCase.test_explicit_definition')
        self.assertEqual(tinc_test_case.author, "balasr3")
        self.assertEqual(tinc_test_case.description, "test case with metadata")
        self.assertEqual(tinc_test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(len(tinc_test_case.tags), 2)
        self.assertSetEqual(tinc_test_case.tags, set(['hashagg', 'orca']))
        self.assertEqual(tinc_test_case.concurrency, 2)
        self.assertEqual(tinc_test_case.iterations, 1)
        
    def test_sanity_run(self):
        test_case = MockConcurrencyTestCase('test_explicit_definition')
        test_case.__class__.__unittest_skip__ = False
        test_case.run()
        # XXX - Do some stuff to prove concurrency
        # The concurrent tests are executed as separate processes since 
        # we are using multi-processing. So, it may not be as simple as
        # incrementing a counter.
    
    def test_multiple_iterations(self):
        test_case = MockConcurrencyTestCase('test_multiple_iterations')
        test_case.__class__.__unittest_skip__ = False
        test_case.run()

    def test_failure(self):
        test_case = MockConcurrencyTestCase('test_failure')
        test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            test_result = TINCTextTestResult(buffer, True, 1)
            test_case.run(test_result)
        self.assertEqual(len(test_result.failures), 1)

    def test_runner(self):
        test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = test_loader.loadTestsFromName('tinctest.models.concurrency.test.test_concurrency_test_case.MockConcurrencyTestCase')
        for test in tinc_test_suite._tests:
            if not 'test_skip' in test.name:
                test.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            tinc_test_runner.run(tinc_test_suite)
        


    
