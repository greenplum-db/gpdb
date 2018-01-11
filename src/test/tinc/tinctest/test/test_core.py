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
from tinctest import TINCTestSuite
from tinctest.runner import TINCTestResultSet, TINCTextTestResult, TINCTestRunner
from tinctest.discovery import TINCDiscoveryQueryHandler

import unittest2 as unittest


"""
We cannot use TINC to test TINC because bugs in TINC may prevent us, during these checks,
from finding the bugs in TINC. Therefore, we must use the more basic unittest mechanisms to
test TINC.
"""

@unittest.skip('mock')
class MockTINCTestCase(tinctest.TINCTestCase):
    """ Mock TINCTestCase used for below unittest TestCases """
    def test_do_stuff(self):
        self.assertTrue(True)

class TINCTestCaseTests(unittest.TestCase):
    def test_sanity_metadata(self):
        tinc_test_case = MockTINCTestCase('test_do_stuff')
        self.assertEqual(tinc_test_case.name, 'MockTINCTestCase.test_do_stuff')
        self.assertEqual(tinc_test_case.author, None)
        self.assertEqual(tinc_test_case.description, '')
        self.assertEqual(tinc_test_case.created_datetime,  datetime.strptime('2000-01-01 00:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2000-01-01 00:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(len(tinc_test_case.tags), 0)

    def test_sanity_run(self):
        tinc_test_case = MockTINCTestCase('test_do_stuff')
        tinc_test_case.run()

    def test_name_attributes(self):
        tinc_test_case = MockTINCTestCase('test_do_stuff')
        self.assertEquals(tinc_test_case.name, 'MockTINCTestCase.test_do_stuff')
        self.assertEquals(tinc_test_case.full_name, 'tinctest.test.test_core.MockTINCTestCase.test_do_stuff')
        self.assertEquals(tinc_test_case.method_name, 'test_do_stuff')
        self.assertEquals(tinc_test_case.class_name, 'MockTINCTestCase')
        self.assertEquals(tinc_test_case.module_name, 'test_core')
        self.assertEquals(tinc_test_case.package_name, 'tinctest.test')

class TINCTestSuiteTests(unittest.TestCase):
    def test_sanity_run(self):
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            tinc_test_suite = tinctest.TINCTestSuite()
            tinc_test_suite.addTest(MockTINCTestCase('test_do_stuff'))
            tinc_test_suite.run(tinc_test_result)

@unittest.skip('mock')
class MockTINCTestCaseWithMetadata(MockTINCTestCase):
    """
    
    @maintainer prabhd
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def test_do_other_stuff(self):
        """
        
        @maintainer prabhd
        @description test *function* (within a class) with metadata
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags orca hashagg regression
        """
        pass
    def test_do_partially_defined_stuff(self):
        """
        @description this is a new description
        @created 2012-07-06 12:00:00
        @modified 2012-07-06 12:00:02
        """
        pass

class TINCTestCaseMetadataTests(unittest.TestCase):
    def test_infer_metadata(self):
        tinc_test_case = MockTINCTestCaseWithMetadata('test_do_stuff')
        self.assertEqual(tinc_test_case.name, 'MockTINCTestCaseWithMetadata.test_do_stuff')
        self.assertEqual(tinc_test_case.author, 'balasr3')
        self.assertEqual(tinc_test_case.maintainer, 'prabhd')
        self.assertEqual(tinc_test_case.description, 'test case with metadata')
        self.assertEqual(tinc_test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.tags, set(['orca', 'hashagg']))
    def test_infer_method_metadata(self):
        tinc_test_case = MockTINCTestCaseWithMetadata('test_do_other_stuff')
        self.assertEqual(tinc_test_case.name, 'MockTINCTestCaseWithMetadata.test_do_other_stuff')
        self.assertEqual(tinc_test_case.author, 'kumara64')
        self.assertEqual(tinc_test_case.maintainer, 'prabhd')
        self.assertEqual(tinc_test_case.description,'test *function* (within a class) with metadata')
        self.assertEqual(tinc_test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual( tinc_test_case.tags, set(['orca', 'hashagg', 'regression']))
    def test_infer_partial_method_metadata(self):
        tinc_test_case = MockTINCTestCaseWithMetadata('test_do_partially_defined_stuff')
        self.assertEqual(tinc_test_case.name, 'MockTINCTestCaseWithMetadata.test_do_partially_defined_stuff')
        self.assertEqual(tinc_test_case.author, 'balasr3')
        self.assertEqual(tinc_test_case.description, 'this is a new description')
        self.assertEqual(tinc_test_case.created_datetime, datetime.strptime('2012-07-06 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2012-07-06 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.tags, set(['orca', 'hashagg']))

@unittest.skip('mock')
class MockTINCTestCaseForResults(tinctest.TINCTestCase):
    def test_success(self):
        self.assertTrue(True)
    def test_failure(self):
        self.assertTrue(False)
    def test_error(self):
        raise Exception()
    def test_skip(self):
        self.skipTest('i feel like it')

class TINCTextTestResultTests(unittest.TestCase):
    def test_success(self):
        tinc_test_case = MockTINCTestCaseForResults('test_success')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        self.assertEqual(tinc_test_result.testsRun, 1)
        self.assertEqual(len(tinc_test_result.failures), 0)
        self.assertEqual(len(tinc_test_result.skipped), 0)
        self.assertEqual(len(tinc_test_result.errors), 0)
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_success \.\.\. .* \.\.\. ok')
    # same for failure
    def test_failure(self):
        tinc_test_case = MockTINCTestCaseForResults('test_failure')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        self.assertEqual(tinc_test_result.testsRun, 1)
        self.assertEqual(len(tinc_test_result.failures), 1)
        self.assertEqual(len(tinc_test_result.skipped), 0)
        self.assertEqual(len(tinc_test_result.errors), 0)
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_failure \.\.\. .* \.\.\. FAIL')
    # same for error
    def test_error(self):
        tinc_test_case = MockTINCTestCaseForResults('test_error')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        self.assertEqual(tinc_test_result.testsRun, 1)
        self.assertEqual(len(tinc_test_result.failures), 0)
        self.assertEqual(len(tinc_test_result.skipped), 0)
        self.assertEqual(len(tinc_test_result.errors), 1)
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_error \.\.\. .* \.\.\. ERROR')
    # same for skip
    def test_skip(self):
        tinc_test_case = MockTINCTestCaseForResults('test_skip')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        self.assertEqual(tinc_test_result.testsRun, 1)
        self.assertEqual(len(tinc_test_result.failures), 0)
        self.assertEqual(len(tinc_test_result.skipped), 1)
        self.assertEqual(len(tinc_test_result.errors), 0)
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_skip \.\.\. .* \.\.\. skipped .*')
    def test_some_combination(self):
        # some combinations of the previous 4, this will require building a test suite
        # and running that test suite with the TINCTextTestResult
        suite = tinctest.TINCTestSuite()
        suite.addTest(MockTINCTestCaseForResults('test_success'))
        suite.addTest(MockTINCTestCaseForResults('test_failure'))
        suite.addTest(MockTINCTestCaseForResults('test_error'))
        suite.addTest(MockTINCTestCaseForResults('test_skip'))
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = TINCTestResultSet(buffer, True, 1)
            suite.run(tinc_test_result)
            text = buffer.getvalue()
        self.assertEqual(tinc_test_result.testsRun, 4)
        self.assertEqual(len(tinc_test_result.failures), 1)
        self.assertEqual(len(tinc_test_result.errors), 1)
        self.assertEqual(len(tinc_test_result.skipped), 1)
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_success \.\.\. .* \.\.\. ok')
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_failure \.\.\. .* \.\.\. FAIL')
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_error \.\.\. .* \.\.\. ERROR')
        self.assertRegexpMatches(text, 'MockTINCTestCaseForResults.test_skip \.\.\. .* \.\.\. skipped .*')

class TINCPulseIntegrationTests(unittest.TestCase):
    p = re.compile('(.*) \((.*)\) \"(.*)\" \.\.\. (.*)\.\d\d ms \.\.\. (\w+)\s?(.*)')
    def test_success(self):
        tinc_test_case = MockTINCTestCaseForResults('test_success')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            # Run tinc test with verbosity=2 similar to Pulse
            tinc_test_result = tinctest.TINCTextTestResult(buffer, descriptions=True, verbosity=2)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        match_object = self.p.match(text)
        self.assertEqual(match_object.group(1), 'MockTINCTestCaseForResults.test_success')
        self.assertEqual(match_object.group(5), 'ok')
    def test_failure(self):
        tinc_test_case = MockTINCTestCaseForResults('test_failure')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            # Run tinc test with verbosity=2 similar to Pulse
            tinc_test_result = tinctest.TINCTextTestResult(buffer, descriptions=True, verbosity=2)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        match_object = self.p.match(text)
        self.assertEqual(match_object.group(1), 'MockTINCTestCaseForResults.test_failure')
        self.assertEqual(match_object.group(5), 'FAIL')
    def test_error(self):
        tinc_test_case = MockTINCTestCaseForResults('test_error')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            # Run tinc test with verbosity=2 similar to Pulse
            tinc_test_result = tinctest.TINCTextTestResult(buffer, descriptions=True, verbosity=2)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        match_object = self.p.match(text)
        self.assertEqual(match_object.group(1), 'MockTINCTestCaseForResults.test_error')
        self.assertEqual(match_object.group(5), 'ERROR')
    def test_skip(self):
        tinc_test_case = MockTINCTestCaseForResults('test_skip')
        tinc_test_case.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            # Run tinc test with verbosity=2 similar to Pulse
            tinc_test_result = tinctest.TINCTextTestResult(buffer, descriptions=True, verbosity=2)
            tinc_test_case.run(tinc_test_result)
            text = buffer.getvalue()
        match_object = self.p.match(text)
        self.assertEqual(match_object.group(1), 'MockTINCTestCaseForResults.test_skip')
        self.assertEqual(match_object.group(5), 'skipped')
        self.assertEqual(match_object.group(6), "'i feel like it'")


@unittest.skip('mock')
class MockTINCTestCaseForLoader(tinctest.TINCTestCase):
    @classmethod
    def setUpClass(cls):
        pass

    @classmethod
    def loadTestsFromTestCase(cls):
        """
        Custom load tests for this test case. Return the two tests
        from this class and check that when we call loadTestsFromName
        for this test case class , these methods will be executed twice
        once that will be loaded by default the unit test loader
        and once as a result of calling loadTestsfromTestCase from within
        loadTestsFromTestCase in TINCTestLoader.
        """
        tests = []
        tests.append(MockTINCTestCaseForLoader('test_0'))
        tests.append(MockTINCTestCaseForLoader('test_1'))
        return tests

    def test_0(self):
        """
        
        @description test test case
        @created 2012-07-05 12:00:00
        @modified 2012-07-08 12:00:02
        @tags tag1 tag2 tag3
        """
        pass

    def test_1(self):
        """
        
        @description test test case
        @created 2012-07-05 12:00:00
        @modified 2012-07-08 12:00:02
        @tags tag1 tag2 tag3
        """
        pass

class TINCTestLoaderTests(unittest.TestCase):
    def test_load_explicit_defined_test_from_name(self):
        """
        Test loadTestsFromName for an already defined python test method
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoader.test_0')
        test_case = test_suite._tests[0]
        self.assertIsNotNone(test_case)
        self.assertEqual(test_case.name, 'MockTINCTestCaseForLoader.test_0')
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['tag1', 'tag2', 'tag3']))

    def test_load_tests_from_class_name(self):
        """
        Test whether the loadTestsFromTestCase defined in the class above is called
        when we provide a class name as an input to the loader's loadTestsFromName.
        loadTestsFromName in TINCTestLoader will call loadTestsFromTestCase in TINCTestLoader
        when the name is a class that can be imported which in turn will look for 'loadTestsFromTestCase'
        in the class to do a custom loading of tests for the test case class.
        Here we test that the above loadTestsFromTestCase in MockTINCTestCaseForLoader
        will be called when we provide MockTINCTestCaseForLoader as an input to loadTestsFromName.

        This should result in four tests loaded as a result of calling loadTestsFromName even though there
        are only two tests in the above class because we are adding additional two tests in the loadTestsFromTestCase
        implementation above.
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoader')
        self.assertIsNotNone(test_suite)
        self.assertEqual(len(test_suite._tests), 4)

    def test_load_invalid_class_name(self):
        test_loader = tinctest.TINCTestLoader()
        with self.assertRaises(AttributeError) as cm:
            test_suite = test_loader.loadTestsFromName('tinctest.test.test_core.blah')
        the_exception = cm.exception
        # Assert the right error message
        expected_error_message = "module object 'tinctest.test.test_core' has no attribute 'blah'"
        self.assertEqual(str(the_exception), expected_error_message)

        # With invalid method
        with self.assertRaises(AttributeError) as cm:
            test_suite = test_loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoader.blah')
        the_exception = cm.exception
        # Assert the right error message
        expected_error_message = "type object 'MockTINCTestCaseForLoader' has no attribute 'blah'"
        self.assertEqual(str(the_exception), expected_error_message)

@unittest.skip('mock')
class MockTINCTestCaseForLoaderDiscovery(tinctest.TINCTestCase):
    def test_lacking_product_version(self):
        """
        
        @maintainer balasr3
        @description test stuff
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags storage executor
        """
        pass

class TINCTestLoaderDiscoveryTests(unittest.TestCase):
    def test_matching_author(self):
        loader = tinctest.TINCTestLoader()
        test_suite = loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoaderDiscovery.test_lacking_product_version')
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['author=pedroc']))
        self.assertEquals(len(filtered_test_suite._tests), 1)
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['author=kumara64']))
        self.assertEquals(len(filtered_test_suite._tests), 0)

    def test_matching_maintainer(self):
        loader = tinctest.TINCTestLoader()
        test_suite = loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoaderDiscovery.test_lacking_product_version')
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['maintainer=balasr3']))
        self.assertEquals(len(filtered_test_suite._tests), 1)
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['maintainer=kumara64']))
        self.assertEquals(len(filtered_test_suite._tests), 0)

    def test_matching_tags(self):
        loader = tinctest.TINCTestLoader()
        test_suite = loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoaderDiscovery.test_lacking_product_version')
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['tags=storage']))
        self.assertEquals(len(filtered_test_suite._tests), 1)
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['tags=kumara64']))
        self.assertEquals(len(filtered_test_suite._tests), 0)

    def test_matching_multiple_tags(self):
        loader = tinctest.TINCTestLoader()
        test_suite = loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseForLoaderDiscovery.test_lacking_product_version')
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['tags=storage', 'tags=executor']))
        self.assertEquals(len(filtered_test_suite._tests), 1)
        filtered_test_suite = loader._filter_and_expand(test_suite, TINCDiscoveryQueryHandler(['tags=storage', 'tags=optimizer']))
        self.assertEquals(len(filtered_test_suite._tests), 0)

    def test_discover_with_invalid_imports(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'data_provider')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['discover_invalid_imports.py'],
                                                    top_level_dir = test_dir)
                                          
        self.assertEquals(len(tinc_test_suite._tests), 1)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            # This should have thrown a ModuleImportFailure error
            self.assertTrue('ModuleImportFailure' in str(tinc_test_result.errors[0][0]))

    def test_discover_multiple_folders(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir1 = os.path.join(pwd, 'folder1')
        test_dir2 = os.path.join(pwd, 'folder2')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir1, test_dir2],
                                                    patterns = ['test*', 'regress*'],
                                                    top_level_dir = None)
        self.assertEquals(len(tinc_test_suite._tests), 4)

    def test_discover_folder_failure(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir3 = os.path.join(pwd, 'folder3_failure')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir3],
                                                    patterns = ['test*', 'regress*'],
                                                    top_level_dir = None,
                                                    query_handler = None)
        # 2 tests for folder1, 2 tests for folder2
        # 4 tests for folder3 test module and regress module from Pass classes
        # 4 actual tests for folder3 test module and regress module from Fail classes shouldn't load, but
        # 2 "tests" for load failure should be there
        test_count = 0
        load_failure_count = 0
        for module in tinc_test_suite._tests:
            for suite in module:
                for test in suite._tests:
                    test_count += 1
                    if 'TINCTestCaseLoadFailure' in str(test):
                        load_failure_count += 1
        self.assertEquals(test_count, 6)
        self.assertEquals(load_failure_count, 2)

    def test_all_tests_loaded(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir1 = os.path.join(pwd, 'dryrun/mocktests')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir1],
                                                    patterns = ['dryrun_test_sample2.py'],
                                                    top_level_dir = None)
        self.assertEquals(tinc_test_suite.countTestCases(), 3)
        flat_test_suite = TINCTestSuite()
        tinc_test_suite.flatten(flat_test_suite)
        for test in flat_test_suite._tests:
            self.assertTrue(test.__class__._all_tests_loaded)

        test_dir1 = os.path.join(pwd, 'dryrun/mocktests')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir1],
                                                    patterns = ['dryrun_test_sample2.py'],
                                                    top_level_dir = None,
                                                    query_handler = TINCDiscoveryQueryHandler('tags=hawq'))
        self.assertEquals(tinc_test_suite.countTestCases(), 1)
        for test in flat_test_suite._tests:
            self.assertFalse(test.__class__._all_tests_loaded)

@unittest.skip('mock')
class MockTINCTestCaseWithCustomResult(tinctest.TINCTestCase):
    """ Mock TINCTestCase that uses its own result
        used for below unittest TestCases """
    def test_success(self):
        self.assertTrue(True)
    def test_failure(self):
        self.assertTrue(False)
    def test_error(self):
        raise Exception
    @unittest.expectedFailure
    def test_expectedfailure(self):
        self.assertTrue(False)
    @unittest.expectedFailure
    def test_unexpectedsuccess(self):
        self.assertTrue(True)
    @unittest.skip('skipping')
    def test_skip(self):
        pass
    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        if stream and descriptions and verbosity:
            return MockTINCTestCaseWithCustomResultResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()
    def run(self, result = None):
        super(MockTINCTestCaseWithCustomResult, self).run(result)
        self._my_result = result

class MockTINCTestCaseWithCustomResultResult(TINCTextTestResult):
    """ The Result class that is used by the test case MockTINCTestCaseWithCustomResult"""
    def __init__(self, stream, descriptions, verbosity):
        self._my_errors = 0
        self._my_pass = 0
        self._my_expected_failure = 0
        self._my_failure = 0
        self._my_unexpected_success = 0
        self._my_skip = 0
        super(MockTINCTestCaseWithCustomResultResult, self).__init__(stream, descriptions, verbosity)

    def startTest(self, test):
        pass

    def addError(self, test, err):
        self._my_errors += 1
        self.result_detail['errors'] = 'errors'
        super(MockTINCTestCaseWithCustomResultResult, self).addError(test, err)

    def addFailure(self, test, err):
        self._my_failure += 1
        self.result_detail['failures'] = 'failures'
        super(MockTINCTestCaseWithCustomResultResult, self).addFailure(test, err)

    def addSuccess(self, test):
        self._my_pass += 1
        self.result_detail['success'] = 'success'
        super(MockTINCTestCaseWithCustomResultResult, self).addSuccess(test)

    def addSkip(self, test, reason):
        self._my_skip += 1
        self.result_detail['skip'] = 'skip'
        super(MockTINCTestCaseWithCustomResultResult, self).addSkip(test, reason)

    def addExpectedFailure(self, test, err):
        self._my_expected_failure += 1
        self.result_detail['expected_failure'] = 'expected_failure'
        super(MockTINCTestCaseWithCustomResultResult, self).addExpectedFailure(test, err)

    def addUnexpectedSuccess(self, test):
        self._my_unexpected_success += 1
        self.result_detail['unexpected_success'] = 'unexpected_success'
        super(MockTINCTestCaseWithCustomResultResult, self).addUnexpectedSuccess(test)

class TINCCustomResultTests(unittest.TestCase):
    """This tests if we construct the appropriate result object for each instance of MockTINCTestCaseWithCustomResult
       when invoked through the runner. Note that to use a custom result object , we have to invoke the tests through
       the runner which will call tinc_test_suite._wrapped_run that will call out to the test case specific defaultTestResult()
       method.
    """
    def test_custom_result_add_success(self):
        test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = test_loader.loadTestsFromName('tinctest.test.test_core.MockTINCTestCaseWithCustomResult')
        for test in tinc_test_suite._tests:
            if not 'test_skip' in test.name:
                test.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            tinc_test_runner.run(tinc_test_suite)
        count = 0
        for test in tinc_test_suite._tests:
            if 'test_success' in test.name:
                self.assertEqual(test._my_result._my_pass, 1)
                self.assertEqual(test._my_result._my_errors, 0)
                self.assertEqual(test._my_result._my_failure, 0)
                self.assertEqual(test._my_result._my_skip, 0)
                self.assertEqual(test._my_result._my_expected_failure, 0)
                self.assertEqual(test._my_result._my_unexpected_success, 0)
                self.assertEqual(len(test._my_result.result_detail), 1)
                self.assertTrue('success' in test._my_result.result_detail)
                count += 1
            if 'test_failure' in test.name:
                self.assertEqual(test._my_result._my_pass, 0)
                self.assertEqual(test._my_result._my_errors, 0)
                self.assertEqual(test._my_result._my_failure, 1)
                self.assertEqual(test._my_result._my_skip, 0)
                self.assertEqual(test._my_result._my_expected_failure, 0)
                self.assertEqual(test._my_result._my_unexpected_success, 0)
                self.assertEqual(len(test._my_result.result_detail), 1)
                self.assertTrue('failures' in test._my_result.result_detail)
                count += 1
            if 'test_error' in test.name:
                self.assertEqual(test._my_result._my_pass, 0)
                self.assertEqual(test._my_result._my_errors, 1)
                self.assertEqual(test._my_result._my_failure, 0)
                self.assertEqual(test._my_result._my_skip, 0)
                self.assertEqual(test._my_result._my_expected_failure, 0)
                self.assertEqual(test._my_result._my_unexpected_success, 0)
                self.assertEqual(len(test._my_result.result_detail), 1)
                self.assertTrue('errors' in test._my_result.result_detail)
                count += 1
            if 'test_expectedfailure' in test.name:
                self.assertEqual(test._my_result._my_pass, 0)
                self.assertEqual(test._my_result._my_errors, 0)
                self.assertEqual(test._my_result._my_failure, 0)
                self.assertEqual(test._my_result._my_skip, 0)
                self.assertEqual(test._my_result._my_expected_failure, 1)
                self.assertEqual(test._my_result._my_unexpected_success, 0)
                self.assertEqual(len(test._my_result.result_detail), 1)
                self.assertTrue('expected_failure' in test._my_result.result_detail)
                count += 1
            if 'test_error' in test.name:
                self.assertEqual(test._my_result._my_pass, 0)
                self.assertEqual(test._my_result._my_errors, 1)
                self.assertEqual(test._my_result._my_failure, 0)
                self.assertEqual(test._my_result._my_skip, 0)
                self.assertEqual(test._my_result._my_expected_failure, 0)
                self.assertEqual(test._my_result._my_unexpected_success, 0)
                self.assertEqual(len(test._my_result.result_detail), 1)
                self.assertTrue('errors' in test._my_result.result_detail)
                count += 1
            if 'test_skip' in test.name:
                self.assertEqual(test._my_result._my_pass, 0)
                self.assertEqual(test._my_result._my_errors, 0)
                self.assertEqual(test._my_result._my_failure, 0)
                self.assertEqual(test._my_result._my_skip, 1)
                self.assertEqual(test._my_result._my_expected_failure, 0)
                self.assertEqual(test._my_result._my_unexpected_success, 0)
                count += 1
                self.assertEqual(len(test._my_result.result_detail), 1)
                self.assertTrue('skip' in test._my_result.result_detail)
        self.assertEqual(count, 6)
