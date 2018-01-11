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

import new
import sys

from qautils.ordereddict import OrderedDict

import tinctest
from tinctest import TINCTestCase
from tinctest.models.concurrency import ConcurrencyTestCase, TINCTestWorkerPool, remote_test_invoker
from tinctest.suite import TINCTestSuite

import unittest2 as unittest

class ScenarioTestCaseException(Exception):
    """
    The exception that is thrown for errors in ScenarioTestCase
    """
    pass

class ScenarioExecutionException(Exception):
    """
    The exception that is thrown for errors that happens during the execution of a scenario
    """
    pass

class _TestCaseScenario(list):
    """
    inherited from list and override the append method in case to have a flag
    to mark if the item should be run sequentially.
    """
    def append(self, item, serial=False):
        super(_TestCaseScenario, self).append((item, serial))

class _Scenario(object):
    """
    Encapsulates a test case scenario containing a list of steps
    """

    def __init__(self, fail_fast=True):
        self.steps = []
        self.fail_fast = fail_fast

    def run(self):
        failure_message = ""
        for step, idx in zip(self.steps, range(len(self.steps))):
            try:
                tinctest.logger.info("Scenario Step : " + str(idx) + " : BEGIN " + "(Serial : %s)" %str(step.is_serial))
                step.run()
                tinctest.logger.info("Scenario Step success: " + str(idx) + " : END")
            except Exception, e:
                tinctest.logger.error("Scenario step %s failed execution: %s" %(str(idx), e))
                tinctest.logger.exception(e)
                tinctest.logger.info("Scenario Step failed: " + str(idx) + " : END")
                if self.fail_fast:
                    tinctest.logger.info("Failing at the first step failure: Scenario step %s failed execution" %str(idx))
                    raise ScenarioExecutionException, "Failing at the first step failure: \n Scenario step %s failed execution: \n %s" %(str(idx), e), sys.exc_info()[2]
                else:
                    failure_message += "\nScenario step %s failed execution: \n%s\n" %(str(idx), e)
        if failure_message:
            raise ScenarioExecutionException(failure_message)

class _ScenarioStep(object):
    """
    Encapsulates a test case scenario step that contains all
    the methods to be executed including the class level fixtures
    """

    def __init__(self, is_serial=False, fail_fast=True):
        self.is_serial = is_serial
        self.fail_fast = fail_fast
        # A dict of test name and a 3-tuple of the test instance, non-keyword arguments,
        # keyword arguments that should be passed to the test invocation
        # Use OrderedDict to remember the order in which the tests are inserted , this is important
        # for seqential steps
        self.tests = OrderedDict()
        self.setup_class_dict = {}
        self.teardown_class_dict = {}

    def length(self):
        return len(self.tests)

    def _execute_setup_class_fixtures(self):
        """
        Run all the setup class fixtures that belong to this step
        """
        for item in self.setup_class_dict:
            try:
                tinctest.logger.info("Executing setUpClass for %s" %item)
                setUpClass = self.setup_class_dict[item]
                setUpClass()
                tinctest.logger.info("Finished executing setUpClass for %s" %item)
            except Exception, e:
                tinctest.logger.error("Failed execution of setUpClass for %s" %item)
                tinctest.logger.exception(e)
                raise ScenarioExecutionException("setUpClass failed for %s: %s" %(item, e))
            
    def _execute_teardown_class_fixtures(self):
        """
        Run all the teardown class fixtures that belong to this step
        """
        failure_msg = ""
        for item in self.teardown_class_dict:
            try:
                tinctest.logger.info("Executing tearDownClass for %s" %item)
                tearDownClass = self.teardown_class_dict[item]
                tearDownClass()
                tinctest.logger.info("Finished executing tearDownCass for %s" %item)
            except Exception, e:
                # Make sure to execute all tearDownClass fixtures before failing the step
                tinctest.logger.error("Failed execution of tearDownClass for %s" %item)
                tinctest.logger.exception(e)
                failure_msg += "tearDownClass failed for %s: %s\n" %(item, e)

        if failure_msg:
            raise ScenarioExecutionException("Failures in tearDownClass fixtures: \n %s" %(failure_msg))
                

    def _execute_tests_sequentially(self):
        """
        Execute all tests belonging to this step sequentially
        """
        failure_message = ""
        for key in self.tests:
            test_name = key
            test = self.tests[key][0]
            non_kw_args = self.tests[key][1]
            kw_args = self.tests[key][2]
            
            my_class = test.__class__.__name__
            my_module = test.__module__
            my_method_name = test._testMethodName

            ret = remote_test_invoker(my_module, my_class, my_method_name, test_name,
                                      dargs=self.tests[test_name][1],
                                      dxargs=self.tests[test_name][2],
                                      setups = [('setUp', [], {})],
                                      cleanups = [('tearDown', [], {})])
            if ret:
                if ret[1].find(tinctest._SKIP_TEST_MSG_PREFIX) != -1:
                    continue
                tinctest.logger.error("Worker %s failed execution \n %s" %(ret[0], ret[1]))
                error_msg = ''
                if ret[1] and len(ret[1].split('\n')) >= 2:
                    error_msg = ret[1].split('\n')[-2]
                if self.fail_fast:
                    raise ScenarioExecutionException("Failing step at the first failure. Worker %s failed execution : %s\n" %(ret[0], error_msg))
                else:
                    failure_message += "Worker %s failed execution : %s \n" %(ret[0], error_msg)

        if failure_message:
            raise ScenarioExecutionException("Workers encountered errors or failures: \n%s" %failure_message)

    def _execute_tests_concurrently(self):
        """
        Execute all tests belonging to this step concurrently
        """
        # If the test case is of type ConcurrencyTestCase, use the same worker pool
        # for concurrent execution
        # TODO - Parameterize maximum pool size
        pool = TINCTestWorkerPool (100)
        for key in self.tests.iterkeys():
            test_name = key
            test = self.tests[key][0]
            non_kw_args = self.tests[key][1]
            kw_args = self.tests[key][2]
            my_class = test.__class__.__name__
            my_module = test.__module__
            my_method_name = test._testMethodName
            if not issubclass(test.__class__, ConcurrencyTestCase):
                pool.submit_test(my_module, my_class, my_method_name, test_name,
                                 dargs=self.tests[test_name][1],
                                 dxargs=self.tests[test_name][2],
                                 setups = [('setUp', [], {})],
                                 cleanups = [('tearDown', [], {})])
            else:
                test.run(result = None, pool = pool)
        pool.join()

        if pool.has_failures():
            raise ScenarioExecutionException("Workers  encountered failures: \n%s" %(pool.get_brief_failure_string()))

    def run(self):
        # First execute all the setup classes
        tinctest.logger.info("Executing setup class fixtures for scenario step")
        self._execute_setup_class_fixtures()
        tinctest.logger.info("Finished executing setup class fixtures for scenario step")

        try:
            if self.is_serial:
                tinctest.logger.info("Executing tests for scenario step sequentially")
                # if this step is marked is_serial, all the test methods will be run sequentially
                self._execute_tests_sequentially()
                tinctest.logger.info("Finished executing tests for scenario step sequentially")
        
            else:
                tinctest.logger.info("Executing tests for scenario step concurrently")
                self._execute_tests_concurrently()
                tinctest.logger.info("Finished executing tests for scenario step concurrently")
        finally:
            # Always execute teardown classes even if there are failures
            tinctest.logger.info("Executing teardown class fixtures for scenario step")
            self._execute_teardown_class_fixtures()
            tinctest.logger.info("Finished executing teardown class fixtures for scenario step")
    

@tinctest.skipLoading("Test model. No tests loaded.")
class ScenarioTestCase(TINCTestCase):
    """
    ScenarioTestCase enables test developers to create a scenario using multiple
    TINCTestCases. A ScenarioTestCase consists of a list of test cases that the
    test developer will populate in their test methods. Each test case added to the
    list must be of type TINCTestCase. Look into test/test_scenario_test_case.py for
    an example on how to construct a scenario.
    The run method in scenario test case will then construct the set of test methods by
    calling TINCTestLoader.loadTestsFromTestCase() and will run all the test methods
    concurrently.
    Note that when a ScenarioTestCase has a ConcurrencyTestCase within a scenario, we share
    the worker pool between scenario test case and the concurrency test case. Processes started
    using multiprocessing.pool cannot instantiate another pool from within.

    @metadata: fail_fast: if set to True, the test case returns with a failure as soon as a test
                          step fails. If set to False, failure is returned only after the entire
                          scenario is executed even when an intermediate test step fails (default: True).
                          
    """


    def __init__(self, methodName="runTest", baseline_result = None):
        # A list of list of test_cases that forms a scenario.
        # All the test cases belonging to the same list will be executed concurrently
        # and each parent list will be executed serially.
        self.test_case_scenario = _TestCaseScenario()

        #: A boolean to indicate if the scenario test case should fail at the first failure
        self.fail_fast = True
        self._scenario = None
        super(ScenarioTestCase, self).__init__(methodName)

    def setUp(self):
        """
        This overriding of setUp clears the scenario steps for a ScenarioTestCase
        before execution
        """
        super(ScenarioTestCase, self).setUp()
        del self.test_case_scenario[0:len(self.test_case_scenario)]

    def _infer_metadata(self):
        super(ScenarioTestCase, self)._infer_metadata()
        if self._metadata.get('fail_fast' , 'True') in ['True' , 'true']:
            self.fail_fast = True
        if self._metadata.get('fail_fast', 'True') in ['False', 'false']:
            self.fail_fast = False

    def run(self, result=None):
        """
        Similar to ConcurrencyTestCase , we construct a new test method with the name
        'self._testMethodName*' that will run the original test method to construct
        the test case scenario and then run the scenario constructed through the
        worker pool. Note that if a test in the scenario is of type ConcurrencyTestCase,
        we share the worker pool between the two. if some list of test cases are marked
        as serial, they will be run sequentially.
        """

        self._orig_testMethodName = self._testMethodName

        def test_function(my_self):
            # First run the original test method to populate test_case_scenario
            orig_test_method = getattr(my_self, my_self._orig_testMethodName)

            # Run the original test method
            orig_test_method()
            
            # Assume self.test_case_scenario is populated correctly after executing the original
            # test method

            # Construct an object of _TestCaseScenario that represents this test case
            my_self._scenario = my_self._construct_scenario(my_self.test_case_scenario, my_self.fail_fast)

            # Run the scenario
            tinctest.logger.info("ScenarioTestCase: " + my_self.name + " : BEGIN")
            try:
                my_self._scenario.run()
            except Exception, e:
                tinctest.logger.error("Scenario test case failed execution: %s" %e)
                tinctest.logger.info("ScenarioTestCase fail: " + my_self.name + " : END")
                my_self.fail("Scenario test case failed execution: %s" %str(e))
            tinctest.logger.info("ScenarioTestCase success: " + my_self.name + " : END")
            

        test_method = new.instancemethod(test_function,
                                         self, self.__class__)
        self.__dict__[ self._testMethodName + "*"] = test_method
        self._testMethodName = self._testMethodName + "*"
        super(ScenarioTestCase, self).run(result)

    def _construct_scenario(self, test_case_scenario, fail_fast):
        """
        @param test_case_scenario: An instance of _TestCaseScenario which is a list of list of test case names
                                   that will define a test case scenario along with a variable defining whether
                                   each step will be run sequentially or not
                                   For eg: [(['test1','test2'], True), (['test3', 'test4'],False)] etc.
        @type test_case_scenario: L{_TestCaseScenario}

        @type fail_fast: boolean
        @param fail_fast: Whether or not a scenario should fail at the first failure. True means when a step fails
                          this scenario will not execute any of the remaining steps.

        @rtype: _Scenario
        @return: An instance of _Scenario that encapsulates the scenario to be executed 

        Reads a scenario test case list names and constructs an appropriate object of _Scenario
        that contains all the methods that should be executed as a part of the scenario
        """
        scenario = _Scenario(fail_fast)
        for test_case_list, step in zip(test_case_scenario, range(len(test_case_scenario))):
            # Construct a step for each item in the list
            scenario_steps = []
            self._construct_scenario_step(test_case_list, fail_fast, scenario_steps)
            for step in scenario_steps:
                scenario.steps.append(step)
        return scenario
        

    def _construct_scenario_step(self, test_case_list, fail_fast, scenario_steps):
        """
        @type test_case_list: tuple
        @param test_case_list: A tuple of list of test case names to be added to the scenario and a boolean specifying
                               whether or not to run this step serially.

        @type fail_fast: boolean
        @type fail_fast: If set to True, a step with sequential steps will terminate at the first failure
        
        Given a list of test case names, this will construct the actual list of test methods with arguments
        that will be executed concurrently for the scenario step.

        In addition, this will also populate the class level fixtures that should be executed before and after
        the execution the scenario step - namely, the setUpClass and tearDownClass fixtures for all the classes
        added to the scenario step.

        Returns a list of steps that would be constructed for a given test case list. If the test case list happens to contain
        a scenario test case , this will expand the contained scenario test case and add the resulting steps from the contained
        scenario test case
        """
        is_serial = test_case_list[1]
        scenario_step = _ScenarioStep(is_serial=is_serial, fail_fast=fail_fast)
        scenario_steps.append(scenario_step)
        
        # Construct a list of all test methods that will be executed concurrently
        for test_case in test_case_list[0]:

            # Form the test case 3-tuple (name, list of non-keyword args, dict of keyword args)
            test_case = self._construct_test_case_tuple(test_case)

            # QAINF-1008: Make the loader not consider the annotation @tinctest.skipLoading
            # This way, the test will not be loaded through a direct load, however will be loaded
            # when it is added to a scenario test case. 
            tincloader = tinctest.TINCTestLoader(consider_skip_loading=False)
            # Load the test into the scenario
            try:
                test_name = None
                if test_case[0].startswith('.'):
                    test_name = self.package_name + test_case[0]
                else:
                    test_name = test_case[0]
                test_suite = tincloader.loadTestsFromName(test_name)
            except Exception, e:
                tinctest.logger.error("Exception while loading test into the scenario - %s" %(test_name))
                tinctest.logger.exception(e)
                raise ScenarioTestCaseException('Exception while loading test into the scenario - %s: %s' %(test_case[0], e))
            
            flat_test_suite = TINCTestSuite()
            self._flatten_test_suite(test_suite, flat_test_suite)
            for test in flat_test_suite:
                if isinstance(test, ScenarioTestCase):
                    # Do not add it to the step if it is a scenario test case
                    # If a contained test case is another ScenarioTestcase, then add the steps of the contained test case
                    # to the current scenario test case
                    test_scenario = self._construct_scenario_for_test(test)
                    for step in test_scenario.steps:
                        scenario_steps.append(step)
                    continue
                
                # This allow duplicate test name to be added in the scenario , and also ordered
                test_key = test.name + "_case_" + str(scenario_step.length())
                scenario_step.tests[test_key] = (test, test_case[1], test_case[2])
                # Add setup class / teardown class fixtures that should be executed for this scenario step
                full_class_name = test.__module__ + '.' + test.__class__.__name__
                setUpClass = getattr(test.__class__, 'setUpClass', None)
                tearDownClass = getattr(test.__class__, 'tearDownClass', None)
                if setUpClass and full_class_name not in scenario_step.setup_class_dict:
                    scenario_step.setup_class_dict[full_class_name] = setUpClass
                if tearDownClass and full_class_name not in  scenario_step.teardown_class_dict:
                    scenario_step.teardown_class_dict[full_class_name] = tearDownClass
        
        return scenario_steps

    def _construct_scenario_for_test(self, test):
        """
        Given an instance of a ScenarioTestCase, this constructs the scenario populated in the test case method.
        """
        # First run the original test method to populate test_case_scenario
        orig_test_method = getattr(test, test._testMethodName)

        # Run the original test method
        orig_test_method()
            
        # Assume self.test_case_scenario is populated correctly after executing the original
        # test method

        # Construct an object of _TestCaseScenario that represents this test case
        scenario = test._construct_scenario(test.test_case_scenario, test.fail_fast)
        return scenario

    def _flatten_test_suite(self, test_suite, flat_test_suite=TINCTestSuite()):
        """
        Given a test suite that may contain other test suites inside, this creates a flat test suite
        that contains just the tests.
        """
        for test in test_suite._tests:
            if isinstance(test, TINCTestSuite):
                self._flatten_test_suite(test, flat_test_suite)
            else:
                flat_test_suite._tests.append(test)
        

    def _construct_test_case_tuple(self, test_case):
        """
        Given an item added to a scenario step, this constructs an appropriate tuple of
        test case name, keyword arguments if any and non-keyword arguments any that should
        be passed during the invocation of the test
        """
        # Instead of just names , users can add tuples of test names with list or dict arguments
        # that represents non-keywords and keyword arguments to a scenario step respectively.
        # For eg: ('test_name', {}) for keyword arguments or ('test_name', []) for non-keyword arguments
        # TBD - Not sure why this was implemented this way -  Why not support both keyword and non-keyword arguments here ? 
            
        # Following checks are performed to see if a test method added to the scenario has arguments

        # if the test_case is just a str, that means the method
        # doesn't have any arguments
        if isinstance(test_case, str):
            test_case = (test_case, [], {})
        # if the test_case is a tuple and the length of it is 1,
        # that means the method doesn't have any arguments
        elif isinstance(test_case, tuple) and len(test_case) == 1:
            test_case = (test_case[0], [], {})

        # if the length of the test_case is 2, that means the
        # method either has arguments or key word arguments
        elif isinstance(test_case, tuple) and len(test_case) == 2:
            if isinstance(test_case[1], list):
                test_case = (test_case[0], test_case[1], {})
            elif isinstance(test_case[1], dict):
                test_case = (test_case[0], [], test_case[1])
            else:
                raise ScenarioTestCaseException("Invalid item added to the scenario - %s" %test_case)
        elif isinstance(test_case, tuple) and len(test_case) == 3:
            if not isinstance(test_case[1], list) or not isinstance(test_case[2], dict):
                raise ScenarioTestCaseException("Invalid item added to the scenario - %s" %test_case)
            test_case = (test_case[0], test_case[1], test_case[2])
        else:
            raise ScenarioTestcaseException("Invalid item added to the scenario - %s" %test_case)
        
        # here , test case would be a 3 tuple consisting of test name , list of non keyword arguments if any
        # dict of keyword arguments if any
        return test_case


    def _consume_scenario_test_case(self, test):
        """
        Given a scenario test case, this method consumes the scenario from the child test case and add's it to
        it's own scenario
        """

