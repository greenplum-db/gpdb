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

import inspect
import os
import re

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator


import tinctest
from tinctest.runner import TINCTestResultSet, TINCTextTestResult, TINCTestRunner

import unittest2 as unittest

@unittest.skip('mock')
class MockTINCTestCaseModelWithoutDecorator(tinctest.TINCTestCase):
    def test_01(self):
        pass
    
@tinctest.skipLoading("model class")
class MockTINCTestCaseModelWithDecorator(tinctest.TINCTestCase):

    def test_01(self):
        pass

@unittest.skip('mock test case')
class MockTINCTestCaseTestsWithoutDecorator(MockTINCTestCaseModelWithoutDecorator):

    def test_02(self):
        pass

class TINCTestLoaderTests(unittest.TestCase):

    def test_loading_without_decorator(self):
        """
        Test that when a parent class has test methods and does not have
        the decorator, we load the method in the base class
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockTINCTestCaseTestsWithoutDecorator)
        self.assertEquals(2, len(test_suite._tests))

    def test_loading_with_decorator(self):
        """
        Test that loader does not load any tests for a test class if
        the decorator is present
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockTINCTestCaseModelWithDecorator)
        self.assertEquals(0, len(test_suite._tests))

    def test_loading_without_decorator_discover(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'skip_loading')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['test_load_without*.py'],
                                                    top_level_dir = test_dir)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run 3 tests as test_01
            # would have been loaded twice
            self.assertEquals(tinc_test_result.testsRun, 3)

    def test_loading_with_decorator_discover(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'skip_loading')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['test_load_with_*.py'],
                                                    top_level_dir = test_dir)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run 2 tests as test_01
            # would have been skipped loading at the base class with decorator
            self.assertEquals(tinc_test_result.testsRun, 2)

        
                
    
