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

from datetime import datetime
import fnmatch
import inspect
import os
from time import sleep

from contextlib import closing
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import unittest2 as unittest
from unittest import TestResult

import tinctest
from tinctest.case import TINCTestCase
from tinctest.models.scenario import ScenarioTestCase
from tinctest.models.concurrency import ConcurrencyTestCase
from tinctest.runner import TINCTextTestResult, TINCTestRunner

t_prefix = 'tinctest.models.scenario.test.test_scenario_test_case.'
@unittest.skip('mock')
class MockScenarioTestCase(ScenarioTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def __init__(self, methodName):
        super(MockScenarioTestCase, self).__init__(methodName)

        #
        # HK / QAINF-108
        # ugly looking hack to get the mocktests to run only during actual test
        # execution from the ScenarioTestCaseTests and not during discovery
        # python reuses modules imported using the same path. importing with
        # different relative paths result in two different executions of the module
        # so, adding something like "MockTestCase1.__unittest_skip__ = False" doesn't
        # work as the variable is only set in the current execution path. When the
        # class is imported from the ScenarioTestCase, it is imported with full path
        # and so it is totally different execution path and the value of the variable
        # set here is not applicable in that context
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase1 as mtc1
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase2 as mtc2
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase3 as mtc3
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase4 as mtc4
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase5 as mtc5
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase6 as mtc6
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase7 as mtc7
        from tinctest.models.scenario.test.test_scenario_test_case import MockConcurrencyTestCase as mctc
        from tinctest.models.scenario.test.scenarios.test_mock_scenarios import MockTINCTestCase as mtc8
        mtc1.__unittest_skip__ = False
        mtc2.__unittest_skip__ = False
        mtc3.__unittest_skip__ = False
        mtc4.__unittest_skip__ = False
        mtc5.__unittest_skip__ = False
        mtc6.__unittest_skip__ = False
        mtc7.__unittest_skip__ = False
        mctc.__unittest_skip__ = False
        mtc8.__unittest_skip__ = False

    def test_serial_execution(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase1')
        test_case_list.append(t_prefix+'MockTestCase2')
        self.test_case_scenario.append(test_case_list, serial=True)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockTestCase2')
        self.test_case_scenario.append(test_case_list2)

    def test_serial_execution_order(self):
        """
        QAINF-976: To test whether a serial step is executed in the same order in which the tests are added to a step
        """
        test_case_list1 = []
        test_case_list1.append(t_prefix + 'MockTestCaseOrder.test_method1')
        test_case_list1.append(t_prefix + 'MockTestCaseOrder.test_method2')
        self.test_case_scenario.append(test_case_list1, serial=True)

    def test_serial_execution_order_failure(self):
        """
        QAINF-976: To test whether a serial step is executed in the same order in which the tests are added to a step
        Assert a failure if the order is reversed. test_method1 should execute before test_method2 for this to pass
        """
        test_case_list1 = []
        test_case_list1.append(t_prefix + 'MockTestCaseOrder.test_method2')
        test_case_list1.append(t_prefix + 'MockTestCaseOrder.test_method1')
        self.test_case_scenario.append(test_case_list1, serial=True)
        

    def test_concurrent_execution(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase1')
        test_case_list.append(t_prefix+'MockTestCase2')
        self.test_case_scenario.append(test_case_list)

    def test_concurrent_execution_with_same_name(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase1')
        test_case_list.append(t_prefix+'MockTestCase1')
        self.test_case_scenario.append(test_case_list)

    def test_scenario_with_concurrency(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockConcurrencyTestCase')
        self.test_case_scenario.append(test_case_list)
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase1')
        self.test_case_scenario.append(test_case_list)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockTestCase2')
        self.test_case_scenario.append(test_case_list2)

    def test_failure(self):
        test_case_list1 = []
        test_case_list1.append(t_prefix+'MockTestCase1')
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockTestCase2')
        test_case_list3 = []
        test_case_list3.append(t_prefix+'MockTestCase3')
        self.test_case_scenario.append(test_case_list1)
        self.test_case_scenario.append(test_case_list2)
        self.test_case_scenario.append(test_case_list3)

    def test_failure_with_fail_fast(self):
        """
        @fail_fast True
        """
        test_case_list1 = []
        test_case_list1.append(t_prefix+'MockTestCase1')
        self.test_case_scenario.append(test_case_list1)
        test_case_list2 = []
        test_case_list2.append(t_prefix + 'MockTestCase3.test_03')
        self.test_case_scenario.append(test_case_list2, True)
        test_case_list3 = []
        test_case_list3.append(t_prefix + 'MockTestCase2')
        self.test_case_scenario.append(test_case_list3)
        test_case_list4 = []
        test_case_list4.append(t_prefix + 'MockTestCase3.test_04')
        self.test_case_scenario.append(test_case_list4)


    def test_failure_with_no_fail_fast(self):
        """
        @fail_fast False
        """
        test_case_list1 = []
        test_case_list1.append(t_prefix+'MockTestCase1')
        self.test_case_scenario.append(test_case_list1)
        test_case_list2 = []
        test_case_list2.append(t_prefix + 'MockTestCase3.test_03')
        self.test_case_scenario.append(test_case_list2, True)
        test_case_list3 = []
        test_case_list3.append(t_prefix + 'MockTestCase2')
        self.test_case_scenario.append(test_case_list3)
        test_case_list4 = []
        test_case_list4.append(t_prefix + 'MockTestCase3.test_04')
        self.test_case_scenario.append(test_case_list4)

    def test_scenario_test_case_with_individual_setup_teardown(self):
        test_case_list1 = []
        test_case_list1.append(t_prefix + 'MockTestCase4')
        self.test_case_scenario.append(test_case_list1)

    def test_serial_concurrency_in_same_execution(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase5')
        test_case_list.append(t_prefix+'MockTestCase6')
        self.test_case_scenario.append(test_case_list, serial=True)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockTestCase5')
        test_case_list2.append(t_prefix+'MockTestCase6')
        self.test_case_scenario.append(test_case_list2)

    def test_non_test_method(self):
        test_case_list = []
        test_case_list.append((t_prefix+'MockTestCase7.non_test_01', [1,2], {'arg3':3}))
        test_case_list.append((t_prefix+'MockTestCase7.non_test_02', [1,2,3]))
        test_case_list.append((t_prefix+'MockTestCase7.non_test_03', {'arg1':3}))
        test_case_list.append(t_prefix+'MockTestCase6')
        self.test_case_scenario.append(test_case_list, serial=True)
        test_case_list2 = []
        test_case_list2.append((t_prefix+'MockTestCase7.non_test_01', [1,2], {'arg3':3}))
        test_case_list2.append((t_prefix+'MockTestCase7.non_test_02', [1,2,3]))
        test_case_list2.append(t_prefix+'MockTestCase6')
        self.test_case_scenario.append(test_case_list2)

    def test_relative_imports(self):
        self.test_case_scenario.append(['.scenarios.test_mock_scenarios.MockTINCTestCase'])

@unittest.skip('mock')
class MockTestCaseOrder(TINCTestCase):
    """
    This test class will be used to test if a serial execution scenario
    is executed in the right order
    """
    @classmethod
    def setUpClass(cls):
        if os.path.exists('/tmp/test_serial_execution_order.txt'):
            os.remove('/tmp/test_serial_execution_order.txt')
            
    def test_method1(self):
        with open('/tmp/test_serial_execution_order.txt', 'w') as f:
            f.write('test')

    def test_method2(self):
        self.assertTrue(os.path.exists('/tmp/test_serial_execution_order.txt'))
                    
        
@unittest.skip('mock')
class MockTestCase1(TINCTestCase):
    def test_01(self):
        self.tested = True
        self.assertTrue(self.tested)

    def test_02(self):
        self.tested = True
        self.assertTrue(self.tested)

@unittest.skip('mock')
class MockTestCase2(TINCTestCase):
    def test_02(self):
        self.tested = True
        self.assertTrue(self.tested)

@unittest.skip('mock')
class MockTestCase3(TINCTestCase):
    def test_03(self):
        self.assertEqual(1, 0, "Simulate failure")

    def test_04(self):
        self.assertEqual(2, 0, "Another failure")

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

    def test_failure(self):
        self.fail("Just like that !")

@unittest.skip('mock')
class MockTestCase4(TINCTestCase):
    def __init__(self, methodName):
        self.pwd = os.path.dirname(inspect.getfile(self.__class__))
        super(MockTestCase4, self).__init__(methodName)

    def setUp(self):
        # touch some file for verification
        with open(os.path.join(self.pwd, 'mc4_setup.tmp'), 'w') as f:
            f.write('setup')

    def test_1(self):
        pass

    def tearDown(self):
        # touch some file for verification
        with open(os.path.join(self.pwd, 'mc4_teardown.tmp'), 'w') as f:
            f.write('teardown')

global_value = 0
@unittest.skip('mock')
class MockTestCase5(TINCTestCase):
    def test_01(self):
        global global_value
        self.tested = True
        self.assertTrue(self.tested)
        global_value +=1
        sleep(2)

    def test_02(self):
        global global_value
        self.tested = True
        self.assertEqual(1, global_value)

@unittest.skip('mock')
class MockTestCase6(TINCTestCase):
    def test_01(self):
        self.tested = True
        self.assertTrue(self.tested)

    def test_02(self):
        self.tested = True
        self.assertTrue(self.tested)

@unittest.skip('mock')
class MockTestCase7(TINCTestCase):
    def non_test_01(self, arg1, arg2, arg3=None):
        self.tested = True
        self.assertTrue(self.tested)
        self.assertIsNotNone(arg1)
        self.assertIsNotNone(arg2)
        self.assertIsNotNone(arg3)

    def non_test_02(self, arg1, arg2, arg3):
        self.tested = True
        self.assertTrue(self.tested)
        self.assertIsNotNone(arg1)
        self.assertIsNotNone(arg2)
        self.assertIsNotNone(arg3)

    def non_test_03(self, arg1=None):
        self.tested = True
        self.assertTrue(self.tested)
        self.assertIsNotNone(arg1)

@unittest.skip('mock')
class MockScenarioTestCaseWithCustomResult(ScenarioTestCase):

    def __init__(self, methodName):
        self.s = 0
        self.f = 0
        super(MockScenarioTestCaseWithCustomResult, self).__init__(methodName)
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase1 as mtc1
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase2 as mtc2
        from tinctest.models.scenario.test.test_scenario_test_case import MockTestCase3 as mtc3
        mtc1.__unittest_skip__ = False
        mtc2.__unittest_skip__ = False
        mtc3.__unittest_skip__ = False

    def test_with_custom_result_pass(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase1')
        self.test_case_scenario.append(test_case_list)
        test_case_list2 = []
        test_case_list2.append(t_prefix+'MockTestCase2')
        self.test_case_scenario.append(test_case_list2)

    def test_with_custom_result_fail(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTestCase3')
        self.test_case_scenario.append(test_case_list)

    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        if stream and descriptions and verbosity:
            return CustomTestCaseResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()


class CustomTestCaseResult(TINCTextTestResult):
    def addSuccess(self, test):
        test.s = 1
        super(CustomTestCaseResult, self).addSuccess(test)

    def addFailure(self, test, err):
        test.f = 1
        super(CustomTestCaseResult, self).addFailure(test, err)

class ScenarioTestCaseTests(unittest.TestCase):
    def test_construction(self):
        tinc_test_case = MockScenarioTestCase('test_serial_execution')
        self.assertEqual(tinc_test_case.name, 'MockScenarioTestCase.test_serial_execution')
        self.assertEqual(tinc_test_case.author, "balasr3")
        self.assertEqual(tinc_test_case.description, "test case with metadata")
        self.assertEqual(tinc_test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(tinc_test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(len(tinc_test_case.tags), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 0)
        self.assertTrue(tinc_test_case.fail_fast)

    def test_construction_with_fail_fast(self):
        tinc_test_case = MockScenarioTestCase('test_failure_with_fail_fast')
        self.assertTrue(tinc_test_case.fail_fast)


    def test_sanity_run(self):
        tinc_test_case = MockScenarioTestCase('test_serial_execution')
        tinc_test_case.__class__.__unittest_skip__ = False
        tinc_test_case.run()
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 1)

    def test_concurrent_execution(self):
        tinc_test_case = MockScenarioTestCase('test_concurrent_execution')
        tinc_test_case.__class__.__unittest_skip__ = False
        tinc_test_case.run()
        self.assertEqual(len(tinc_test_case.test_case_scenario), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)

    def test_concurrent_execution_with_same_name(self):
        tinc_test_case = MockScenarioTestCase('test_concurrent_execution_with_same_name')
        tinc_test_case.__class__.__unittest_skip__ = False
        tinc_test_case.run()
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)

    def test_failure_with_fail_fast(self):
        # make sure we exit at the first failure
        tinc_test_case = MockScenarioTestCase('test_failure_with_fail_fast')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(result.testsRun, 1)
        self.assertEqual(len(result.failures), 1)
        # The failure message should not include test_04 here
        self.assertTrue('MockTestCase3.test_03' in result.failures[0][1])
        self.assertFalse('MockTestCase3.test_04' in result.failures[0][1])
        self.assertEqual(len(tinc_test_case.test_case_scenario), 4)
        self.assertEqual(len(result.errors), 0)

    def test_failure_with_no_fail_fast(self):
        # make sure we do not exit at the first failure and continue till the end
        tinc_test_case = MockScenarioTestCase('test_failure_with_no_fail_fast')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(result.testsRun, 1)
        self.assertEqual(len(result.failures), 1)
        # The failure message should include test_04 here
        self.assertTrue('MockTestCase3.test_03' in result.failures[0][1])
        self.assertTrue('MockTestCase3.test_04' in result.failures[0][1])
        self.assertEqual(len(tinc_test_case.test_case_scenario), 4)

    def test_scenario_with_concurrency_fail(self):
        # make scenario test fail at first error
        tinc_test_case = MockScenarioTestCase('test_scenario_with_concurrency')
        tinc_test_case.__class__.__unittest_skip__ = False
        results = TestResult()
        tinc_test_case.run(results)

        # there are 3 tests; 1st one should fail and return
        self.assertEqual(results.testsRun, 1)
        self.assertEqual(len(results.failures), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 3)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 1)

    def test_sanity_run_failure(self):
        tinc_test_case = MockScenarioTestCase('test_failure')
        tinc_test_case.__class__.__unittest_skip__ = False
        results = TestResult()
        tinc_test_case.run(results)
        self.assertEqual(len(results.failures), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 3)

    def test_scenario_with_individual_setup_teardown(self):
        tinc_test_case = MockScenarioTestCase('test_scenario_test_case_with_individual_setup_teardown')
        tinc_test_case.__class__.__unittest_skip__ = False
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        for fn in os.listdir(pwd):
            if fnmatch.fnmatch(fn, 'mc4_*.tmp'):
                os.remove(os.path.join(pwd, fn))
        self.assertFalse(os.path.exists(os.path.join(pwd, 'mc4_setup.tmp')))
        self.assertFalse(os.path.exists(os.path.join(pwd, 'mc4_teardown.tmp')))
        results = TestResult()
        tinc_test_case.run(results)
        self.assertEquals(len(results.failures), 0)
        self.assertEquals(len(results.errors), 0)
        self.assertEquals(results.testsRun, 1)
        self.assertTrue(os.path.exists(os.path.join(pwd, 'mc4_setup.tmp')))
        self.assertTrue(os.path.exists(os.path.join(pwd, 'mc4_teardown.tmp')))

    def test_scenario_with_custom_result_pass(self):
        test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = test_loader.loadTestsFromName('tinctest.models.scenario.test.test_scenario_test_case.MockScenarioTestCaseWithCustomResult.test_with_custom_result_pass')
        for test in tinc_test_suite._tests:
            test.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            tinc_test_runner.run(tinc_test_suite)

        for test in tinc_test_suite._tests:
            if 'test_with_custom_result_pass' in test.name:
                self.assertEqual(test.s, 1)
                self.assertEqual(test.f, 0)

    def test_scenario_with_custom_result_fail(self):
        test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = test_loader.loadTestsFromName('tinctest.models.scenario.test.test_scenario_test_case.MockScenarioTestCaseWithCustomResult.test_with_custom_result_fail')
        for test in tinc_test_suite._tests:
            test.__class__.__unittest_skip__ = False
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            tinc_test_runner.run(tinc_test_suite)

        for test in tinc_test_suite._tests:
            if 'test_with_custom_result_fail' in test.name:
                self.assertEqual(test.s, 0)
                self.assertEqual(test.f, 1)
    def test_serial_concurrency_in_same_execution(self):
        tinc_test_case = MockScenarioTestCase('test_serial_concurrency_in_same_execution')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 2)
        self.assertEqual(len(result.failures), 0)


    def test_non_test_method_run(self):
        tinc_test_case = MockScenarioTestCase('test_non_test_method')
        tinc_test_case.__class__.__unittest_skip__ = False
        tinc_test_case.run()
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 4)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 3)

    def test_serial_execution_order(self):
        """
        Test whether tests added to a serial step are executed in the same order in which they are added
        """
        tinc_test_case = MockScenarioTestCase('test_serial_execution_order')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(result.failures), 0)

    def test_serial_execution_order_failure(self):
        """
        Test whether tests added to a serial step are executed in the same order in which they are added
        This should fail since test_method1 should execute before test_method2
        """
        tinc_test_case = MockScenarioTestCase('test_serial_execution_order_failure')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(result.failures), 1)

    def test_relative_imports(self):
        """
        Test whether relative imports work properly. Scenarios can be added relative to the ScenarioTestCase
        by prefixing the test case name with '.'
        """
        tinc_test_case = MockScenarioTestCase('test_relative_imports')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 1)
        self.assertEqual(len(result.failures), 0)
        

@unittest.skip('Mock')
class MockTINCTestCaseWithClassFixtures(tinctest.TINCTestCase):
    setup_class = 0
    teardown_class = 0

    @classmethod
    def setUpClass(cls):
        cls.setup_class += 1

    def test_step1_01(self):
        self.assertEquals(self.setup_class, 1)
        self.assertEquals(self.teardown_class, 0)

    def test_step1_02(self):
        self.assertEquals(self.setup_class, 1)
        self.assertEquals(self.teardown_class, 0)

    def test_step2_03(self):
        self.assertEquals(self.setup_class, 2)
        self.assertEquals(self.teardown_class, 1)

    def test_step2_04(self):
        self.assertEquals(self.setup_class, 2)
        self.assertEquals(self.teardown_class, 1)

    @classmethod
    def tearDownClass(cls):
        cls.teardown_class += 1

@unittest.skip('Mock')
class MockAnotherTINCTestCaseWithClassFixtures(tinctest.TINCTestCase):
    setup_class = 0
    teardown_class = 0

    @classmethod
    def setUpClass(cls):
        cls.setup_class += 1

    def test_step1_01(self):
        self.assertEquals(self.setup_class, 1)
        self.assertEquals(self.teardown_class, 0)

    def test_step1_02(self):
        self.assertEquals(self.setup_class, 1)
        self.assertEquals(self.teardown_class, 0)

    def test_step2_03(self):
        self.assertEquals(self.setup_class, 2)
        self.assertEquals(self.teardown_class, 1)

    def test_step2_04(self):
        self.assertEquals(self.setup_class, 2)
        self.assertEquals(self.teardown_class, 1)

    @classmethod
    def tearDownClass(cls):
        cls.teardown_class += 1

@unittest.skip('mock')
class MockTINCTestCaseWithSetUpClassFailure(tinctest.TINCTestCase):

    @classmethod
    def setUpClass(cls):
        raise Exception("Failing setup class")

    def test_01(self):
        self.assertTrue(True)


@unittest.skip('mock')
class MockTINCTestCaseWithTearDownClassFailure(tinctest.TINCTestCase):

    @classmethod
    def tearDownClass(cls):
        raise Exception("Failing teardown class")

    def test_01(self):
        self.assertTrue(True)


@unittest.skip('mock')
class MockScenarioTestCaseWithClassFixtures(ScenarioTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    def __init__(self, methodName):
        super(MockScenarioTestCaseWithClassFixtures, self).__init__(methodName)

        from tinctest.models.scenario.test.test_scenario_test_case import MockTINCTestCaseWithClassFixtures as mtc1
        from tinctest.models.scenario.test.test_scenario_test_case import MockAnotherTINCTestCaseWithClassFixtures as mtc2
        mtc1.__unittest_skip__ = False
        mtc2.__unittest_skip__ = False

    def test_step_with_class_fixtures(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_02')
        self.test_case_scenario.append(test_case_list)

    def test_multiple_steps_with_class_fixtures(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_02')
        self.test_case_scenario.append(test_case_list)
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_03')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_04')
        self.test_case_scenario.append(test_case_list)

    def test_step_with_multiple_class_fixtures(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_02')
        test_case_list.append(t_prefix+'MockAnotherTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockAnotherTINCTestCaseWithClassFixtures.test_step1_02')
        self.test_case_scenario.append(test_case_list)

    def test_multiple_steps_with_multiple_class_fixtures(self):
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_02')
        test_case_list.append(t_prefix+'MockAnotherTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockAnotherTINCTestCaseWithClassFixtures.test_step1_02')
        self.test_case_scenario.append(test_case_list)
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_03')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_04')
        test_case_list.append(t_prefix+'MockAnotherTINCTestCaseWithClassFixtures.test_step2_03')
        test_case_list.append(t_prefix+'MockAnotherTINCTestCaseWithClassFixtures.test_step2_04')
        self.test_case_scenario.append(test_case_list)

    def test_step_with_setup_class_failure(self):
        test_case_list = []
        test_case_list.append(t_prefix + 'MockTINCTestCaseWithSetUpClassFailure')
        self.test_case_scenario.append(test_case_list)

    def test_step_with_teardown_class_failure(self):
        test_case_list = []
        test_case_list.append(t_prefix + 'MockTINCTestCaseWithTearDownClassFailure')
        self.test_case_scenario.append(test_case_list)

    def test_multiple_steps_with_setup_class_failure_between(self):
        """
        @fail_fast False
        """
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_02')
        self.test_case_scenario.append(test_case_list)
        # Introduce a step with setup class failure
        test_case_list = []
        test_case_list.append(t_prefix + 'MockTINCTestCaseWithSetUpClassFailure')
        self.test_case_scenario.append(test_case_list)
        # This step should be executed with fail fast set to False
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_03')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_04')
        self.test_case_scenario.append(test_case_list)

    def test_multiple_steps_with_teardown_class_failure_between(self):
        """
        @fail_fast False
        """
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_01')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step1_02')
        self.test_case_scenario.append(test_case_list)
        # Introduce a step with setup class failure
        test_case_list = []
        test_case_list.append(t_prefix + 'MockTINCTestCaseWithTearDownClassFailure')
        self.test_case_scenario.append(test_case_list)
        # This step should be executed even with fail fast set to False
        test_case_list = []
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_03')
        test_case_list.append(t_prefix+'MockTINCTestCaseWithClassFixtures.test_step2_04')
        self.test_case_scenario.append(test_case_list)



class ScenarioTestsWithClassFixtures(unittest.TestCase):

    def test_step_with_class_fixtures(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_step_with_class_fixtures')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0

        tinc_test_case.run(result)
        # Verify that setup class and teardown class was executed only once
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 1)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(result.failures), 0)


    def test_multiple_steps_with_class_fixtures(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_multiple_steps_with_class_fixtures')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)
        # Verify that setup class and teardown class was executed twice, once for each step
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 2)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 2)
        self.assertEqual(len(result.failures), 0)

    def test_step_with_multiple_class_fixtures(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_step_with_multiple_class_fixtures')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)
        # Verify that setup class and teardown class for both classes was executed once
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 1)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 1)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.teardown_class, 1)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.setup_class, 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 1)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 4)
        self.assertEqual(len(result.failures), 0)

    def test_multiple_steps_with_multiple_class_fixtures(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_multiple_steps_with_multiple_class_fixtures')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)
        # Verify that setup class and teardown class for both classes was executed once, for each step
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 2)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 2)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.teardown_class, 2)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockAnotherTINCTestCaseWithClassFixtures.setup_class, 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario), 2)
        self.assertEqual(len(tinc_test_case.test_case_scenario[0][0]), 4)
        self.assertEqual(len(tinc_test_case.test_case_scenario[1][0]), 4)
        self.assertEqual(len(result.failures), 0)

    def test_step_with_setup_class_failure(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_step_with_setup_class_failure')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(result.failures), 1)
        self.assertTrue('setUpClass failed for' in result.failures[0][1])

    def test_step_with_teardown_class_failure(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_step_with_teardown_class_failure')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinc_test_case.run(result)
        self.assertEqual(len(result.failures), 1)
        self.assertTrue('tearDownClass failed for' in result.failures[0][1])

    def test_multiple_steps_with_setup_class_failure_between(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_multiple_steps_with_setup_class_failure_between')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)

        # Verify that setup class and teardown class is executed are executed twice for steps 0 and 2
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 2)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 2)

        self.assertEqual(len(result.failures), 1)
        self.assertTrue('setUpClass failed for' in result.failures[0][1])

    def test_multiple_steps_with_teardown_class_failure_between(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_multiple_steps_with_teardown_class_failure_between')
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)

        # Verify that setup class and teardown class is executed twice for steps 0 and 2
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 2)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 2)

        self.assertEqual(len(result.failures), 1)
        self.assertTrue('tearDownClass failed for' in result.failures[0][1])

    def test_multiple_steps_with_setup_class_failure_between_with_fail_fast(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_multiple_steps_with_setup_class_failure_between')
        tinc_test_case.fail_fast = True
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)

        # Verify that setup class and teardown class is executed only once with fail fast set to True
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 1)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 1)

        self.assertEqual(len(result.failures), 1)
        self.assertTrue('setUpClass failed for' in result.failures[0][1])


    def test_multiple_steps_with_teardown_class_failure_between_with_fail_fast(self):
        tinc_test_case = MockScenarioTestCaseWithClassFixtures('test_multiple_steps_with_teardown_class_failure_between')
        tinc_test_case.fail_fast = True
        tinc_test_case.__class__.__unittest_skip__ = False
        result = TestResult()
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class = 0
        tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class = 0
        tinc_test_case.run(result)

        # Verify that setup class and teardown class are executed only once with fail fast set to True
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.teardown_class, 1)
        self.assertEquals(tinctest.models.scenario.test.test_scenario_test_case.MockTINCTestCaseWithClassFixtures.setup_class, 1)

        self.assertEqual(len(result.failures), 1)
        self.assertTrue('tearDownClass failed for' in result.failures[0][1])



