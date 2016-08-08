import inspect
import os
import re

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import tinctest.test.dryrun

from tinctest.runner import TINCTestResultSet, TINCTextTestResult, TINCTestRunner
from tinctest.discovery import TINCDiscoveryQueryHandler

import unittest2 as unittest

class TINCDryRunTests(unittest.TestCase):

    def setUp(self):
        # Reset the variable manipulated in tests
        tinctest.test.dryrun.tests_run_count = 0

    def test_execution_with_dryrun(self):
        """
        Test that the actual tests are not executed in dryrun mode
        """
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'mocktests')

        
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['dryrun_test_sample2.py'],
                                                    top_level_dir = test_dir,
                                                    dryrun=True
                                                    )


        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            self.assertEquals(tinc_test_result.testsRun, 3)
            # This variable should be 0 even after the tests are run in the dryrun mode since
            # we do not expect to run the test code
            self.assertEquals(tinctest.test.dryrun.tests_run_count, 0)


        #Without dryrun it should have set the variable

        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['dryrun_test_sample2.py'],
                                                    top_level_dir = test_dir
                                                    )
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            self.assertEquals(tinc_test_result.testsRun, 3)
            # tests should have been run and tests_run_count should be incremented
            self.assertEquals(tinctest.test.dryrun.tests_run_count, 8)

    def test_skipped_tests_with_dryrun(self):
        """
        Test that dryrun reports tests with skip metadata without running setup class / setup / teardownclass/ teardown fixtures
        """
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'mocktests')

        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['dryrun_test_sample2.py'],
                                                    top_level_dir = test_dir,
                                                    dryrun=True
                                                    )

        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            self.assertEquals(tinc_test_result.testsRun, 3)
            # This variable should be 0 even after the tests are run in the dryrun mode since
            # we do not expect to run the test code
            self.assertEquals(tinctest.test.dryrun.tests_run_count, 0)

            #test03 in the result should be skipped correctly
            self.assertEquals(len(tinc_test_result.skipped),1)
            self.assertEquals(tinc_test_result.skipped[0][1],
                              "just skipping")

    def test_module_import_failures_with_dryrun(self):
        """
        Test that module import failures are reported correctly in dryrun
        """
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'mocktests')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['dryrun_test_invalid_import.py'],
                                                    top_level_dir = test_dir,
                                                    dryrun=True
                                                    )


        
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            self.assertEquals(tinc_test_result.testsRun, 1)
            # This variable should be 0 even after the tests are run in the dryrun mode since
            # we do not expect to run the test code
            self.assertEquals(tinctest.test.dryrun.tests_run_count, 0)

            self.assertEquals(len(tinc_test_result.errors),1)

            test_instance = tinc_test_result.errors[0][0]
            traceback_msg = tinc_test_result.errors[0][1]
            self.assertEquals(test_instance.__class__.__name__, "TINCModuleImportFailure")
            self.assertTrue("Failed to import test module" in traceback_msg)

    def test_class_load_failure_with_dryrun(self):
        """
        Test that class load failure is reported correctly in dryrun
        """
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'mocktests')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['dryrun_test_load_failure.py'],
                                                    top_level_dir = test_dir,
                                                    dryrun=True
                                                    )


        
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            self.assertEquals(tinc_test_result.testsRun, 1)
            # This variable should be 0 even after the tests are run in the dryrun mode since
            # we do not expect to run the test code
            self.assertEquals(tinctest.test.dryrun.tests_run_count, 0)

            # There should be one error in the test suite
            self.assertEquals(len(tinc_test_result.errors),1)

            test_instance = tinc_test_result.errors[0][0]
            traceback_msg = tinc_test_result.errors[0][1]
            self.assertEquals(test_instance.__class__.__name__, "TINCTestCaseLoadFailure")
            self.assertTrue("Invalid product version" in traceback_msg)


    def test_filtered_tests_with_dryrun(self):
        """
        Test that tests filtered out with tinc queries are reported correctly in dryrun
        """
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'mocktests')
        query_handler = TINCDiscoveryQueryHandler("tags != hawq")
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir],
                                                    patterns = ['dryrun_test_sample2.py'],
                                                    top_level_dir = test_dir,
                                                    query_handler = query_handler,
                                                    dryrun = True
                                                    )


        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            self.assertEquals(tinc_test_result.testsRun, 3)
            # This variable should be 0 even after the tests are run in the dryrun mode since
            # we do not expect to run the test code
            self.assertEquals(tinctest.test.dryrun.tests_run_count, 0)

            # Two tests should be skipped one because of skip metadata and one because of filtering out

            self.assertEquals(len(tinc_test_result.skipped), 2)

            metadata_skip_test_tuple = None
            queries_skip_test_tuple = None
            # tinc_test_result.skipped will be a list of tuple containing test instance and the skip message
            for test_tuple in tinc_test_result.skipped:
                if test_tuple[0]._testMethodName == 'test_03':
                    metadata_skip_test_tuple = test_tuple
                elif test_tuple[0]._testMethodName == 'test_01':
                    queries_skip_test_tuple = test_tuple

            self.assertIsNotNone(metadata_skip_test_tuple)
            self.assertIsNotNone(queries_skip_test_tuple)

            self.assertTrue("Filtering out test as it did not satisy tinc queries check" in queries_skip_test_tuple[1])
            self.assertTrue("just skipping" in metadata_skip_test_tuple[1])
            
        

