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

from contextlib import closing
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import tinctest

from tinctest import TINCException, TINCTestSuite
from tinctest.lib import local_path
from tinctest.discovery import TINCDiscoveryQueryHandler

import unittest2 as unittest

@unittest.skip('Mock test case')
class MockTINCTestCaseWithDataProvider(tinctest.TINCTestCase):
    def test_with_no_data_provider(self):
        self.assertTrue(True)
    
    def test_with_data_provider(self):
        """
        @data_provider data_types_provider
        """
        self.assertTrue(True)

@unittest.skip('Mock test case')
class MockTINCTestCaseWithDataProviderComplicated(tinctest.TINCTestCase):
    def test_with_data_provider_complicated(self):
        """
        @data_provider data_types_provider data_types_provider_string data_types_provider_dict
        """
        self.assertTrue(True)

@unittest.skip('Mock test case')
class MockTINCTestCaseWithDataProviderFailure(tinctest.TINCTestCase):

    def __init__(self, baseline_result = None):
        self.current_test_data = None
        super(MockTINCTestCaseWithDataProviderFailure, self).__init__(baseline_result)

    def test_failure_with_data_provider(self):
        """
        @data_provider data_types_provider
        """
        file_name = self.test_data[0] + '.out'
        with open(local_path(file_name), 'w') as f:
            f.write(self.test_data[0])
        self.assertTrue(False)

    def test_with_invalid_data_provider(self):
        """
        @data_provider invalid_data_provider
        """
        self.assertTrue(True)

    def test_with_data_provider_returning_none(self):
        """
        @data_provider none_data_provider
        """
        self.assertTrue(True)

    def test_with_data_provider_returning_empty_dict(self):
        """
        @data_provider empty_data_provider
        """
        self.assertTrue(True)

    def test_with_data_provider_returning_non_dict(self):
        """
        @data_provider data_provider_returning_non_dict
        """
        self.assertTrue(True)


@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {'type1': ['int', 'int2'], 'type2': ['varchar']}
    return data

@tinctest.dataProvider('data_types_provider_string')
def test_data_provider_strig():
    data = {'type3': "string", 'type4': "boolean", 'type5': "char"}
    return data

@tinctest.dataProvider('data_types_provider_dict')
def test_data_provider_dict():
    data = {'type6': {"key":"value"}}
    return data

@tinctest.dataProvider('none_data_provider')
def none_data_provider():
    return None

@tinctest.dataProvider('empty_data_provider')
def empty_data_provider():
    return {}

@tinctest.dataProvider('data_provider_returning_non_dict')
def data_provider_returning_non_dict():
    data = ['type1', 'type2', 'type3']
    return data


class TINCTestCaseWithDataProviderTests(unittest.TestCase):
    def test_with_data_provider_construction(self):
        tinc_test_case = MockTINCTestCaseWithDataProvider('test_with_data_provider')
        self.assertEquals(tinc_test_case.data_provider, 'data_types_provider')


    def test_suite_construction_with_data_provider(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider')
        #This should have constructed three test methods
        self.assertEquals(len(test_suite._tests), 3)
        for tinc_test in test_suite._tests:
            # The name of the generated methods for the data provider test methods should be
            # <orig_test_method_name>_key
            self.assertTrue(tinc_test._testMethodName == 'test_with_no_data_provider' or
                            tinc_test._testMethodName == 'test_with_data_provider_type1' or
                            tinc_test._testMethodName == 'test_with_data_provider_type2')

            if tinc_test._testMethodName == 'test_with_data_provider_type1' or \
                    tinc_test._testMethodName == 'test_with_data_provider':
                self.assertIsNotNone(tinc_test.test_data_dict)
                self.assertEquals(tinc_test._orig_testMethodName, 'test_with_data_provider')


            if tinc_test._testMethodName == 'test_with_data_provider_type1':
                self.assertIsNotNone(tinc_test.test_data_dict)

            if tinc_test._testMethodName == 'test_with_data_provider_type2':
                self.assertIsNotNone(tinc_test.test_data_dict)


    def test_suite_construction_with_data_provider2(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider')
        #This should have constructed three test methods
        self.assertEquals(len(test_suite._tests), 3)
        for tinc_test in test_suite._tests:
            # The name of the generated methods for the data provider test methods should be
            # <orig_test_method_name>_key
            self.assertTrue(tinc_test._testMethodName == 'test_with_no_data_provider' or
                            tinc_test._testMethodName == 'test_with_data_provider_type1' or
                            tinc_test._testMethodName == 'test_with_data_provider_type2')

            if tinc_test._testMethodName == 'test_with_data_provider_type1' or \
                    tinc_test._testMethodName == 'test_with_data_provider':
                self.assertIsNotNone(tinc_test.test_data_dict)
                self.assertEquals(tinc_test._orig_testMethodName, 'test_with_data_provider')

    def test_suite_construction_with_data_provider3(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider.test_with_data_provider')
        #This should have constructed three test methods
        self.assertEquals(len(test_suite._tests), 2)
        for tinc_test in test_suite._tests:
            # The name of the generated methods for the data provider test methods should be
            # <orig_test_method_name>_key
            self.assertTrue(tinc_test._testMethodName == 'test_with_data_provider_type1' or
                            tinc_test._testMethodName == 'test_with_data_provider_type2')

            if tinc_test._testMethodName == 'test_with_data_provider_type1' or \
                    tinc_test._testMethodName == 'test_with_data_provider':
                self.assertIsNotNone(tinc_test.test_data_dict)
                self.assertEquals(tinc_test._orig_testMethodName, 'test_with_data_provider')

    def test_suite_construction_test_full_name(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider')
        self.assertEquals(len(test_suite._tests), 3)
        for tinc_test in test_suite._tests:
            # The name of the generated methods for the data provider test methods should be
            # <orig_test_method_name>_key
            self.assertTrue(tinc_test.full_name == 'tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider.test_with_no_data_provider' or
                            tinc_test.full_name == 'tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider.test_with_data_provider_type1' or
                            tinc_test.full_name == 'tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider.test_with_data_provider_type2')


    def test_run_test_with_data_provider(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider.test_with_data_provider')
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run two tests
            self.assertEquals(tinc_test_result.testsRun, 2)

    def test_run_test_with_data_provider_no_expand(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProvider.test_with_data_provider', expand = False)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run one test, since expand is False
            self.assertEquals(tinc_test_result.testsRun, 1)

    def test_run_test_with_data_provider_verify_data(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderFailure.test_failure_with_data_provider')
        for test in tinc_test_suite._tests:
            test.__class__.__unittest_skip__ = False

        pwd = os.path.dirname(inspect.getfile(self.__class__))
        for file in os.listdir(pwd):
            if fnmatch.fnmatch(file, '*.out'):
                os.remove(os.path.join(pwd, file))

        test_file1 = os.path.join(pwd, 'type1.out')
        test_file2 = os.path.join(pwd, 'type2.out')
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run two tests
            self.assertEquals(tinc_test_result.testsRun, 2)
            self.assertEquals(len(tinc_test_result.failures), 2)
            self.assertTrue(os.path.exists(test_file1))
            self.assertTrue(os.path.exists(test_file2))

    def test_with_invalid_data_provider(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = None
        with self.assertRaises(TINCException) as cm:
            tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderFailure.test_with_invalid_data_provider')
        self.assertIsNone(tinc_test_suite)

    def test_with_data_provider_returning_none(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = None
        with self.assertRaises(TINCException) as cm:
            tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderFailure.test_with_data_provider_returning_none')
        self.assertIsNone(tinc_test_suite)

    def test_with_data_provider_returning_empty_dict(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = None
        with self.assertRaises(TINCException) as cm:
            tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderFailure.test_with_data_provider_returning_empty_dict')
        self.assertIsNone(tinc_test_suite)

    def test_with_data_provider_returning_non_dict(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = None
        with self.assertRaises(TINCException) as cm:
            tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderFailure.test_with_data_provider_returning_non_dict')
        self.assertIsNone(tinc_test_suite)

    def test_suite_construction_with_discover(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'data_provider')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['test_*.py'],
                                                    top_level_dir = test_dir)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run 11 tests
            self.assertEquals(tinc_test_result.testsRun, 11)

    def test_suite_construction_with_discover_and_tinc_queries(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'data_provider')
        query_handler = TINCDiscoveryQueryHandler("tags=tag1")
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['test_*.py'],
                                                    top_level_dir = test_dir,
                                                    query_handler = query_handler)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have filtered 4 tests and hence run 7 tests
            self.assertEquals(tinc_test_result.testsRun, 7)

    def test_suite_construction_with_discover_and_no_expand(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'data_provider')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['test_*.py'],
                                                    top_level_dir = test_dir,
                                                    expand = False)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run 8 tests, since data provider shouldn't be expanded
            self.assertEquals(tinc_test_result.testsRun, 8)

    def test_suite_construction_with_data_provider_complicated(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderComplicated')
        # There are 3 data providers with these possible values (sorted by data provider name):
        # data_types_provider: type1, type2
        # data_types_provider_dict: type6
        # data_types_provider_string: type3, type4, type5
        # A test case will be created for all combinations of the above keys:
        # type1_type6_type3, type1_type6_type4, type1_type6_type5,
        # type2_type6_type3, type2_type6_type4, type2_type6_type5,
        #This should have constructed 6 test methods
        self.assertEquals(len(test_suite._tests), 6)
        for tinc_test in test_suite._tests:
            # The name of the generated methods for the data provider test methods should be
            # <orig_test_method_name>_key
            # All the keys are sorted by their data_provider name!
            self.assertTrue(tinc_test._testMethodName == 'test_with_data_provider_complicated_type1_type6_type3' or
                            tinc_test._testMethodName == 'test_with_data_provider_complicated_type1_type6_type4' or
                            tinc_test._testMethodName == 'test_with_data_provider_complicated_type1_type6_type5' or
                            tinc_test._testMethodName == 'test_with_data_provider_complicated_type2_type6_type3' or
                            tinc_test._testMethodName == 'test_with_data_provider_complicated_type2_type6_type4' or
                            tinc_test._testMethodName == 'test_with_data_provider_complicated_type2_type6_type5')

            self.assertIsNotNone(tinc_test.test_data_dict)
            self.assertEquals(tinc_test._orig_testMethodName, 'test_with_data_provider_complicated')

    def test_run_test_with_data_provider_complicated(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = tinc_test_loader.loadTestsFromName('tinctest.test.test_data_provider.MockTINCTestCaseWithDataProviderComplicated.test_with_data_provider_complicated')
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have run two tests
            self.assertEquals(tinc_test_result.testsRun, 6)
