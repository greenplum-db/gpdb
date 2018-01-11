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

import copy
import new
import random
import sys
import os
import time
import traceback

import unittest2 as unittest
import unittest2.loader

from unittest2.loader import _CmpToKey
from unittest2.loader import _make_failed_import_test

import tinctest

from tinctest.case import TINCTestCase
from tinctest.suite import TINCTestSuite

# Create test cases for failed loading.
# This is similar to unittest, except the test case created is tinctest.TINCTestCase, instead of unittest.TestCase.
def make_failed_test(classname, methodname, exception):
    if classname is None:
        classname = "TINCTestCaseLoadFailure"
    tinctest.logger.error("Error loading test %s.%s." % (classname, methodname))
    tinctest.logger.exception(exception)
    def testFailure(self):
        raise exception
    attrs = {methodname: testFailure}
    TestClass = type(classname, (tinctest.TINCTestCase,), attrs)
    return TestClass(methodname)

def _make_tinc_failed_import_test(name, suiteClass):
    message = 'Failed to import test module: %s' %name
    tinctest.logger.error("Error importing module %s" %name)
    if hasattr(traceback, 'format_exc'):
        # Python 2.3 compatibility
        # format_exc returns two frames of discover.py as well
        message += '\n%s' % traceback.format_exc()

    def testFailure(self):
        raise ImportError(message)
    attrs = {name: testFailure}
    TestClass = type('TINCModuleImportFailure', (tinctest.TINCTestCase,), attrs)
    return suiteClass((TestClass(name),))

class TINCTestLoader(unittest.TestLoader):
    """
    TINCTestLoader is built atop unittest's TestLoader to
    implement a robust way of loading test cases that is returned
    as a TINCTestSuite.
    """
    #: Default test method prefix. Only tests having this prefix will be loaded.
    testMethodPrefix = 'test_'
    #: Default test suite class that should be used to construct tests. Defaults to TINCTestSuite
    suiteClass = TINCTestSuite

    def __init__(self, consider_skip_loading=True):
        self.total_tests = 0
        self.filtered_tests = 0
        self.consider_skip_loading=consider_skip_loading
        super(TINCTestLoader, self).__init__()

    
    def loadTestsFromTestCase(self, testCaseClass):
        """
        Responsible for loading test cases that belongs to any test class inheriting from TINCTestCase.
        By default, this uses the unittest loader for loading test cases. In addition to the default behavior,
        if the test case class has 'loadTestsFromTestCase' defined and implemented, this will call out to the testCaseClass
        for custom loading of tests for the test case class.

        If the test case class is decorated with 'skipLoading', this will ignore loading for the particular test case class.

        @param testCaseClass: Class object of the test case for which the loading should be performed.
        @type testCaseClass: TINCTestCase class object

        @param consider_skip_loading: boolean that tells whether or not to consider the annotation tinctest.skipLoading
        @type consider_skip_loading: boolean

        @rtype: TINCTestSuite
        @return: Tests loaded for the test case class wrapped in a TINCTestSuite
        """
        # Set all tests loaded to True at the beginning of loading a test class
        testCaseClass._all_tests_loaded = True
        
        # If test case class has the annotation @tinctest.skipLoading, just return
        if self.consider_skip_loading and hasattr(testCaseClass, '__tinc_skip_loading__') and \
               testCaseClass.__tinc_skip_loading__:
            # Check class name and source file to see if it is not inherited
            current_class_name = testCaseClass.__name__
            current_file = sys.modules[testCaseClass.__module__].__file__

            if testCaseClass.__tinc_skip_loading_declared_class__ == current_class_name and \
               testCaseClass.__tinc_skip_loading_declared_file__ == current_file:

                tinctest.logger.info("Skipping test loading for test class %s: %s" %(testCaseClass.__name__,
                                                                                 testCaseClass.__tinc_skip_loading_msg__))
                return TINCTestSuite()
            
        tinctest.logger.debug("Loading tests for test class: %s.%s" %(testCaseClass.__module__, testCaseClass.__name__))
        try:
            test_suite = super(TINCTestLoader, self).loadTestsFromTestCase(testCaseClass)
        except Exception, e:
            # This means that the class did not load properly
            # Add a failure test case to our suite
            test_suite = TINCTestSuite()
            test_suite.addTest(make_failed_test(None, testCaseClass.__module__ + "." + testCaseClass.__name__, e))
            return test_suite
        
        if not issubclass(testCaseClass, TINCTestCase):
            return test_suite

        # look for a loadTestsFromTestCase in the testCaseClass. this would
        # be a class method, surely. if such a class method is available, defer to
        # it entirely.
        try:
            loadTestsFromClass = getattr(testCaseClass, 'loadTestsFromTestCase')
            clazz_tests = loadTestsFromClass()
            for test in clazz_tests:
                test_suite.addTest(test)
        except AttributeError:
            # This means that the class does not have a loadTestsFromTestCase and we merely return
            # an equivalent TINCTestSuite()
            pass
        except Exception, e:
            # This means that the class did not load properly
            # Add a failure test case to our suite
            test_suite.addTest(make_failed_test(None, testCaseClass.__module__ + "." + testCaseClass.__name__, e))
        
        tinctest.logger.debug("Finished loading tests for test class: %s.%s" %(testCaseClass.__module__, testCaseClass.__name__))
        return test_suite

    def loadTestsFromName(self, name, module=None, dryrun=False, expand=True):
        """
        For dynamically generated test methods, loadTestsFromName from unittest.loader will
        raise an error. We handle the case where the test method is not yet generated in this
        implementation of loadTestsFromName.

        @type name: string
        @param name: Name of the test to be loaded. Could be path to a class, module, test method

        @rtype: TINCTestSuite
        @return: The tests loaded for the given name wrapped in a TINCTestSuite
        """
        test_suite = TINCTestSuite()

        # The following block of code is taken from unittest2.loader.loadTestsFromName.
        # It finds the biggest subset from the given name that can be successfully imported.
        parts = name.split('.')
        if module is None:
            parts_copy = parts[:]
            while parts_copy:
                try:
                    m = __import__('.'.join(parts_copy))
                    break
                except ImportError:
                    del parts_copy[-1]
                    if not parts_copy:
                        raise
            parts = parts[1:]

        obj = m

        # We exclude the last part from the name which could potentially be a test method
        # that is not generated yet.
        for part in parts[:-1]:
            parent, obj = obj, getattr(obj, part)

        try:
            parent, obj = obj, getattr(obj, parts[-1])
        except AttributeError, e:
            # This means that the last part is a method which is not yet generated.
            # So we construct an instance of the test class which will dynamically
            # add the test method to its instance.
            #if not issubclass(obj, SQLTestCase):
            #    raise TypeError("don't know how to make test from: %s" % obj)
            try:
                klass = getattr(parent, parts[-2])
                test = klass(parts[-1])
                test_suite.addTest(test)
            except:
                raise AttributeError("%s object \'%s\' has no attribute \'%s\'"%(type(klass).__name__,
                                                                                 klass.__name__,
                                                                                 parts[-1]))
        else:
            # This means everything is defined and we just call the parent.
            test_suite = super(TINCTestLoader, self).loadTestsFromName(name, module)

        return self._filter_and_expand(test_suite, dryrun=dryrun, expand=expand)

    def discover(self, start_dirs, patterns, top_level_dir, query_handler=None, dryrun=False, expand=True):
        """
        Discovers all tests from all start directories provided.
        Tests will be filtered using the query handler.
        @type start_dirs: list
        @param start_dirs: List of directories from where tests should be discovered

        @type patterns: list
        @param patterns: Patterns of test modules from which tests should be discovered.

        @type query_handler: l{TINCDiscoveryQueryHandler}
        @param query_handler: The query handler object that will be used to filter tests based on tinc queries.

        @type dryrun: boolean
        @param dryrun: Boolean to determine whether it is a dry run. True means that no tests will be executed.

        @rtype: TINCTestSuite
        @return: All tests discovered wrapped in a TINCTestSuite
        """
        self.total_tests = 0
        self.filtered_tests = 0
        discovery_start_time = time.time()
        tinctest.logger.status("Started test discovery")

        # Set dryrun for the main test suite that will be returned through
        # discovery
        
        all_tests = TINCTestSuite(dryrun=dryrun)

        for start_dir in start_dirs:
            discover_dir_start_time = time.time()
            tinctest.logger.info("Started discovering tests from %s" %start_dir)
            for pattern in patterns:
                pattern_load_start_time = time.time()
                tinctest.logger.info("Started discovering tests from %s with pattern %s" %(start_dir, pattern))
                self._top_level_dir = None
                # monkey-patch _make_failed_import_test in unittest2.loader to use tinctest.loader's _make_tinc_failed_import_test
                # this is to generate a tinc test case for module import failures that can easily be reported upon in the dry run
                unittest2.loader._make_failed_import_test = _make_tinc_failed_import_test
                test_suite = super(TINCTestLoader, self).discover(start_dir, pattern, top_level_dir)
                pattern_load_elapsed = (time.time() - pattern_load_start_time) * 1000
                tinctest.logger.info("Finished loading tests from %s with pattern %s in %4.2f ms" %(start_dir, pattern,
                                                                                                    pattern_load_elapsed))

                tinctest.logger.info("Started applying tinc queries for tests from %s with pattern %s" %(start_dir, pattern))
                apply_queries_start_time = time.time()
                for test in test_suite._tests:
                    all_tests.addTest(self._filter_and_expand(test, query_handler, dryrun=dryrun, expand=expand))
                apply_queries_elapsed = (time.time() - apply_queries_start_time) * 1000
                tinctest.logger.info("Total tests before querying: %s Total tests after querying: %s" %(self.total_tests,
                                                                                                        self.filtered_tests))
                
                tinctest.logger.info("Finished applying tinc queries for tests from %s with pattern %s in %4.2f ms" %(start_dir,
                                                                                                                      pattern,
                                                                                                                      apply_queries_elapsed
                                                                                                                      ))
                pattern_discover_elapsed = (time.time() - pattern_load_start_time) * 1000
                tinctest.logger.info("Finished discovering tests from %s with pattern %s in %4.2f ms" %(start_dir, pattern,
                                                                                                         pattern_discover_elapsed))
            discover_dir_elapsed = (time.time() - discover_dir_start_time) * 1000
            tinctest.logger.info("Finished discovering tests from %s in %4.2f ms" %(start_dir, discover_dir_elapsed))

        discovery_elapsed = (time.time() - discovery_start_time) * 1000
        tinctest.logger.status("Finished test discovery in %4.2f ms. Total tests before querying: %s Total tests after querying: %s" \
                               %(discovery_elapsed,
                                 self.total_tests,
                                 self.filtered_tests))
        return all_tests
    

    def _filter_and_expand(self, test_suite, query_handler=None, dryrun=False, expand=True):
        """
        Filters tests based on tinc queries and expands data provider tests

        Filter:
        If tinc queries are provided (-q option), then filter out any tests case
        that does not satisfy query

        Expand: 
        Inspects each test in the given test suite and looks for tests with data 
        providers and expands the test suite to dynamically add new test methods 
        for the given data provider. Also removes the main data provider test 
        from the suite after expanding which should not be run.
        """
        expanded_filtered = TINCTestSuite(dryrun=dryrun)

        # QAINF-920, all the filtered tests (test not satisfying tinc queries)
        # will not be added to the resulting test suite during normal execution. This is to avoid having the runner execute
        # setUpClass fixtures for test classes that are completely filtered. These tests will not show in test reports
        # as there may be schedules filtering out a lot of tests. Users are expected to do a dryrun to figure out which tests
        # are filtered out because of tinc queries check.
        # TODO: May be show all these tests in normal execution as well in a verbose reporting mode. 
        # Note that the skip flag will be handled in TINCTestCase.setUp() since these tests are explicitly skipped by users
        # and we might want to show them out in test reports. 
        
        for test in test_suite._tests:
            # Recurse inside if test suite
            if isinstance(test, unittest.TestSuite):
                expanded_filtered._tests.append(self._filter_and_expand(test, query_handler, dryrun=dryrun, expand=expand))
                continue

            # If it is just a test case object:
            #  - if it has a data_provider, expand the test and remove the 
            #    main data_provider test
            #  - apply tinc queries
            if issubclass(test.__class__, TINCTestCase):
                self.total_tests += 1
                if not self._filter_test_case(test, query_handler):
                    if dryrun:
                        expanded_filtered.addTest(test)
                    # no need to anything more since this doesn't satisfy filter checks
                    continue
                self.filtered_tests += 1
                tinctest.logger.debug("Succeeded tinc filter checks. Adding test: %s" %test.full_name)

                if expand and test.data_provider:
                    tinctest.logger.debug("dataprovider configured. Expanding test %s" %test.full_name)
                    data_provider_tests = test.add_data_provider_test_methods()
                    for method in data_provider_tests:
                        # the following code makes a copy of the test function
                        # with a side effect that all the instance variables
                        # are also copied as a reference. That is, the original
                        # test function and the test function both will have 
                        # references to the same instace variable object. modifying
                        # the variable in one will affect the other
                        # TODO: check if deepcopy can be done
                        new_test = copy.copy(test)
                        new_test._testMethodName = method
                        new_test.full_name = new_test._get_full_name()
                        expanded_filtered.addTest(new_test)
                else:
                    expanded_filtered.addTest(test)
            else:
                expanded_filtered.addTest(test)

            
        return expanded_filtered

    def _filter_test_case(self, test_case, query_handler):
        """
        Given a test case , verify if the test case passes the following checks
        """
        if query_handler:
            tinctest.logger.debug("Applying tinc queries to test: %s" %test_case.full_name)
            satisfies_queries = query_handler.apply_queries(test_case)
            if not satisfies_queries:
                # Set this flag only for failed queries check since this is the only check
                # that will impact tincmm
                test_case.__class__._all_tests_loaded = False
                tinctest.logger.debug("Failed tinc queries check : %s" %test_case.full_name)
                test_case.skip = "Filtering out test as it did not satisy tinc queries check"
                return False

        return True
    
    @staticmethod
    def findTINCTestFromName(name):
        """
        From a given name, determine the test and return a TINCTestCase object.
        The test return should be callable from laodTestsFromName.
        Data provider test case will have a name of pkg.mod.test_<orig>_<key1>_<key2>.
        This method should return pkg.mod.test_<orig>.
        
        @type name: string
        @param name: Name of the test to extract callable TINCTestCase from.

        @rtype: TINCTestCase
        @return: The original test (non-implicit test) from the given name
        """

        # The following block of code is taken from loadTestsFromName
        # It finds the biggest subset from the given name that can be successfully imported.
        parts = name.split('.')
        parts_copy = parts[:]
        while parts_copy:
            try:
                m = __import__('.'.join(parts_copy))
                break
            except ImportError:
                del parts_copy[-1]
                if not parts_copy:
                    raise
        parts = parts[1:]
        
        obj = m
        
        # We exclude the last part from the name which could potentially be a test method
        # that is not generated yet.
        for part in parts[:-1]:
            parent, obj = obj, getattr(obj, part)
        
        # At this point, we have the class name and the method name
        module_object = parent
        class_object = obj
        class_name = parts[-2]
        method_name = parts[-1]
        
        # Split the method_name by _ and see how far along we can load.
        # For example, if the method name is test_query01_dataprovider1key,
        # see if we can load that before trying test_query01.
        
        method_parts = method_name.split('_')
        
        test_case_object = None
        while method_parts:
            # See if method is explicit and callable as is
            try:
                new_method_name = '_'.join(method_parts)
                test_case_object = class_object(new_method_name)
                break
            except Exception:
                # Need to go on with the loop and see if it works after stripping the last _
                del method_parts[-1]
        if not test_case_object:
            raise AttributeError("Class %s has no callable method %s" % (class_name, method_name))
        return test_case_object

class TINCTestLoaderReverse(TINCTestLoader):
    """
    A reverse test loader that implements loadTestsFromTestCase to load
    tests in a reverse alphabetical order.
    """

    def loadTestsFromTestCase(self, testCaseClass):
        """
        This implementation of loadTestsFromTestCase loads tests in a reverse
        alphabetical order for the given test case class.
        
        @param testCaseClass: Test case class object for which the test loading should happen.
        @type testCaseClass: class object

        @rtype: TINCTestSuite
        @return: All tests loaded for the test case class wrapped in a TINCTestSuite.
        """
        test_suite = super(TINCTestLoaderReverse, self).loadTestsFromTestCase(testCaseClass)

        test_case_names = []
        for test in test_suite._tests:
            test_case_names.append(test._testMethodName)

        if self.sortTestMethodsUsing:
            test_case_names.sort(key=_CmpToKey(self.sortTestMethodsUsing), reverse=True)

        if not test_case_names and hasattr(testCaseClass, 'runTest'):
            test_case_names = ['runTest']

        loaded_suite = self.suiteClass(map(testCaseClass, test_case_names))
        return loaded_suite

   
            
class TINCTestLoaderRandomized(TINCTestLoader):
    """
    A randomized test loader that implements loadTestsFromTestCase to load
    tests in a randomized alphabetical order.
    """
    def loadTestsFromTestCase(self, testCaseClass):
        """
        This implementation of loadTestsFromTestCase loads tests in a randomized
        alphabetical order for the given test case class.
        
        @param testCaseClass: Test case class object for which the test loading should happen.
        @type testCaseClass: class object

        @rtype: TINCTestSuite
        @return: All tests loaded for the test case class wrapped in a TINCTestSuite
        """
        test_suite = super(TINCTestLoaderRandomized, self).loadTestsFromTestCase(testCaseClass)
        test_case_names = []
        for test in test_suite._tests:
            test_case_names.append(test._testMethodName)

        random.shuffle(test_case_names)    

        if not test_case_names and hasattr(testCaseClass, 'runTest'):
            test_case_names = ['runTest']

        loaded_suite = self.suiteClass(map(testCaseClass, test_case_names))
        return loaded_suite
