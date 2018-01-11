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

import os
import socket
import time

import unittest2 as unittest

import tinctest

class TINCTestRunner(unittest.TextTestRunner):
    """
    The default test runner used in tinc. This makes sure we use TINC specific
    result objects with tinc runs. By default, unittest's test runner uses a
    unit test result object and we override some internal methods in this class
    to patch a TINCTestResultSet for a TINCTestSuite.
    """
    def _makeResult( self ):
        return TINCTestResultSet(self.stream, self.descriptions, self.verbosity)


class TINCTextTestResult(unittest.TextTestResult):
    """
    TINC test result class that can print formatted text results to a stream.
    Our normal and verbose mode is verbose mode in unittest and our quiet mode is normal mode in unittest, 
    hence the verbosity value is increased. 
    """
    def __init__(self, stream, descriptions, verbosity):
        super(TINCTextTestResult, self).__init__(stream, descriptions, verbosity+1)
        self.start_time = 0
        self.end_time = 0
        self.duration = ''
        self.result_string = ''
        self.result_message = ''
        self.value = 0
        self.result_detail = {}
        self.output = ''
        self.stack_trace = ''
        self.tinc_normal_output = verbosity == 1
        self.verbosity = verbosity

    def getDescription(self, test):
        """
        Return a brief description of the test object as a string

        @param test: An instance of TINCTestCase
        @type test: TINCTestCase

        @rtype: string
        @return: A brief description of the test case. Uses TINCTestCase's description variable.
        """
        method_name, full_name = [name.strip('*') for name in test.__str__().split()]
        suite_name, class_name = full_name.strip('()').rsplit('.',1)
        case_name = class_name + '.' + method_name
        if self.tinc_normal_output:
            return case_name
        return '%s (%s) "%s"' % (case_name, suite_name, test.description)

    def startTest(self, test):
        """
        This implementation of startTest sets start time for test objects. 

        @param test: An instance of TINCTestCase
        @type test: TINCTestCase
        """
        super(TINCTextTestResult, self).startTest(test)
        self.start_time = test.start_time = time.time()
        tinctest.logger.separator()
        tinctest.logger.status("Started test: %s : %s" %(test.full_name, test.description))

    def stopTest(self, test):
        """
        This implementation of stopTest sets the result_message instance variable for failed / errored tests.

        @param test: An instance of TINCTestCase
        @type test: TINCTestCase
        """
        if self.stack_trace:
            tinctest.logger.exception("Stack trace: %s" %self.stack_trace)
        result_message = self.result_message if self.result_message else "NONE"
        tinctest.logger.status("Finished test: %s Result: %s Duration: %s Message: %s" %(test.full_name,
                                                                                       self.result_string,
                                                                                       self.duration,
                                                                                       result_message))
        tinctest.logger.separator()
        super(TINCTextTestResult, self).stopTest(test)

    def addSuccess(self, test):
        self.end_time = test.end_time = time.time()
        self.result_string = 'PASS'
        self._show_run_time(test)
        super(TINCTextTestResult, self).addSuccess(test)

    def addError(self, test, err):
        self.result_string = 'ERROR'
        self.result_message = err[1]
        self._collect_files(test)
        self.stack_trace = self._exc_info_to_string(err, test).strip()
        self._show_run_time(test)
        super(TINCTextTestResult, self).addError(test, err)

    def addFailure(self, test, err):
        self.result_message = err[1]
        self.stack_trace = self._exc_info_to_string(err, test).strip()
        if self.stack_trace.find(tinctest._SKIP_TEST_MSG_PREFIX) != -1:
            # if we are in addFailure and see a message for skipped test
            # in the message, it is coming a thread (eg, concurrenty test)
            # handle it separately
            pass
        else:
            self.result_string = 'FAIL'
            self._collect_files(test)
            self._show_run_time(test)
            super(TINCTextTestResult, self).addFailure(test, err)

    def addExpectedFailure(self, test, err):
        self.end_time = test.end_time = time.time()
        self.result_string = 'PASS'
        self._show_run_time(test)
        super(TINCTextTestResult, self).addExpectedFailure(test, err)

    def addUnexpectedSuccess(self, test):
        self.end_time = test.end_time = time.time()
        self.result_string = 'FAIL'
        self.result_message = 'Unexpected success for the test marked as expected failure'
        self._show_run_time(test)
        super(TINCTextTestResult, self).addUnexpectedSuccess(test)

    def addSkip(self, test, err):
        self.result_string = 'SKIP'
        self.result_message = err
        self._show_run_time(test)
        super(TINCTextTestResult, self).addSkip(test, err)

    def _show_run_time(self, test):
        self.end_time = test.end_time = time.time()
        elapsed = self.end_time - self.start_time
        self.duration = test.duration = "%4.2f ms" %(elapsed*1000)
        if not self.dots:
            self.stream.write("%s ... " %self.duration)

    def _collect_files(self, test):
        if hasattr(test, 'collect_files'):
            test.collect_files()

class TINCTestResultSet(TINCTextTestResult):
    """
    A default test result set object that will be used with TINCTestRunner for running
    an instance of a TINCTestSuite. 
    """
    
    def __init__(self, stream, descriptions, verbosity):
        self.run_results = list()
        super(TINCTestResultSet, self).__init__(stream, descriptions, verbosity)

    def addResult(self, result):
        self.run_results.append(result)
        self.testsRun += result.testsRun
        if result.skipped:
            self.skipped.extend(result.skipped)
        if result.failures:
            if result.__class__.__name__ == 'TestResult':
                self._print_fail_summary(result.failures[0], 'FAIL')
            self.failures.extend(result.failures)
        if result.errors:
            if result.__class__.__name__ == 'TestResult':
                self._print_fail_summary(result.errors[0], 'ERROR')
            self.errors.extend(result.errors)
        if result.expectedFailures:
            self.expectedFailures.extend(result.expectedFailures)
        if result.unexpectedSuccesses:
            self.unexpectedSuccesses.extend(result.unexpectedSuccesses)

    def _print_fail_summary(self, res_tuple, fail_type = 'ERROR'):
        '''
        QAINF-875
        When non-test case specific errors/failures (they are errors mainly) occur,
        unittest constructs a new test object for the error (ModuleImportError object,
        for example) in the result. If we get here, it is because of the existence
        of such a "test". In order to output some status that pulse can parse, we
        are going to use this function. We will assume time taken to be 0ms.
        '''
        self.stream.write(res_tuple[0]._testMethodName)
        self.stream.write(" ... ")
        self.stream.write("0.0 ms")
        self.stream.write(" ... ")
        self.stream.writeln(fail_type)
        tinctest.logger.status("Finished test: %s Result: %s Duration: %s Message: %s" %(res_tuple[0]._testMethodName,
                                                                                       fail_type,
                                                                                       "0.00 ms",
                                                                                       res_tuple[1]))
