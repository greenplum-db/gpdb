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

from tinctest.runner import TINCTestResultSet
from tinctest.case import TINCTestCase

class TINCTestSuite(unittest.TestSuite):
    """
    A base TINC test suite that are constrcuted by the tinc loaders and that
    can be passed to tinc test runners. TINC test loaders and runners understands only
    TINCTestSuite-s which is an extension of unittest.TestSuite.

    For use, create an instance of TINCTestSuite, then add test case instances - of type TINCTestCase.
    When all tests have been added, the suite can be passed to a test
    runner, such as TINCTestRunner.

    In addition to what unittest does , TINCTestSuite knows how to call custom result objects defined
    for a TINC test case. Each TINCTestCase can define it's own result object and expose it through it's
    defaultTestResult method which will be used by TINCTestSuite to call the specific result object apis like
    addSuccess, addFailure etc configured in it's custom result object. 
    """

    def __init__(self, tests=(), dryrun=False):
        super(TINCTestSuite, self).__init__(tests)
        # Easier to add this as an instance variable than providing
        # an argument to TestSuite's run method. The latter requires
        # us to override the runner's run method as well which is what
        # calls test suite run. 
        self.dryrun = dryrun

    def __str__(self):
        return "{dryrun=%s: %s}" %(str(self.dryrun), super(TINCTestSuite, self).__str__())

    def flatten(self, flat_test_suite):
        """
        This utility allows user to flatten self. All the suites within self's tests will be flattened and added to flat_test_suite
        """
        for test in self._tests:
            if isinstance(test, TINCTestSuite):
                test.flatten(flat_test_suite)
            else:
                flat_test_suite._tests.append(test)
            
    def _wrapped_run(self, result, debug=False):
        if self.dryrun:
            self._dryrun(result)
            return
        
        for test in self:
            if result.shouldStop:
                break

            if unittest.suite._isnotsuite(test):
                run_result = test.defaultTestResult()
                self._tearDownPreviousClass(test, result)
                self._handleModuleFixture(test, result)
                self._handleClassSetUp(test, result)
                result._previousTestClass = test.__class__

                if (getattr(test.__class__, '_classSetupFailed', False) or
                    getattr(result, '_moduleSetUpFailed', False)):
                    continue

                if issubclass(result.__class__, TINCTestResultSet) and \
                        issubclass(test.__class__, TINCTestCase):
                    run_result = test.defaultTestResult(result.stream,
                                                        result.descriptions,
                                                        result.verbosity)
            else:
                if issubclass(result.__class__, TINCTestResultSet):
                    run_result = TINCTestResultSet(result.stream, 
                                                   result.descriptions, 
                                                   result.verbosity)

            if hasattr(test, '_wrapped_run'):
                test._wrapped_run(result, debug)
                if issubclass(result.__class__, TINCTestResultSet):
                    result.addResult(run_result)
                else:
                    result.testsRun += run_result.testsRun
                    if run_result.failures:
                        result.failures.extend(run_result.failures)
                    if run_result.errors:
                        result.errors.extend(run_result.errors)
                    if run_result.skipped:
                        result.skipped.extend(run_result.skipped)
            elif not debug:
                test(run_result)
                if issubclass(result.__class__, TINCTestResultSet):
                    result.addResult(run_result)
                else:
                    result.testsRun += run_result.testsRun
                    if run_result.failures:
                        result.failures.extend(run_result.failures)
                    if run_result.errors:
                        result.errors.extend(run_result.errors)
                    if run_result.skipped:
                        result.skipped.extend(run_result.skipped)
            else:
                test.debug()

    def _dryrun(self, result):
        """
        Runs the given test or test suite in dryrun mode. No tests will be run, just import and load failures will be reported
        along with tests that are skipped.
        """
        for test in self:
            if hasattr(test, '_dryrun'):
                test._dryrun(result)
            else:
                # If it is an actual test and if the test does not have any skip metadata associated with it, run the test,
                # This will just be tests with module import failures and test case load failures.
                if test.__class__.__name__ == 'TINCTestCaseLoadFailure' or test.__class__.__name__ == 'TINCModuleImportFailure':
                    test(result)

                elif issubclass(test.__class__, TINCTestCase):
                    result.startTest(test)
                    if test.skip:
                        result.addSkip(test, test.skip)
                    else:
                        result.addSuccess(test)
                    
                    result.stopTest(test)

                elif test.__class__.__name__ == 'ModuleImportFailure':
                    test(result)

    def flatten(self, flat_test_suite):
        """
        This utility allows user to flatten self. All the suites within self's tests will be flattened and added to flat_test_suite
        """
        for test in self._tests:
            if isinstance(test, TINCTestSuite):
                test.flatten(flat_test_suite)
            else:
                flat_test_suite._tests.append(test)

