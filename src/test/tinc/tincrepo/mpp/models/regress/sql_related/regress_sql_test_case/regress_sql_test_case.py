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
import tinctest

from tinctest.lib import local_path
from tinctest.runner import TINCTestRunner

from mpp.models import SQLTestCase, SQLTestCaseException

import unittest2 as unittest

import shutil

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

# we're testing SQLTestCase as it pertains to tinc.py (and only tinc.py)
# as such, any attempts by raw unit2 to discover and load MockSQLTestCase must be averted
@unittest.skip('mock')
class MockSQLTestCase(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    db_name=os.getenv('USER')
    def test_explicit_test_method(self):
        pass

@unittest.skip('mock')
class MockSQLTestCaseGenerateAns(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    sql_dir = 'sql_no_ans/'
    generate_ans = 'yes'
    def test_explicit_test_method(self):
        pass
    
@unittest.skip('mock')
class MockSQLTestCaseForceGenerateAns(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    sql_dir = 'sql_no_ans/'
    generate_ans = 'force'
    def test_explicit_test_method(self):
        pass
    
@unittest.skip('mock')
class MockSQLTestCaseIncorrectGenerateAns(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """

    # Misspelled generate_ans. Invalid value.
    generate_ans = 'yess'
    def test_explicit_test_method(self):
        pass

@unittest.skip('mock')
class MockSQLTestCaseGpdiffNoAnsFile(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    sql_dir = 'sql_no_ans/'
    def test_explicit_test_method(self):
        pass

@unittest.skip('mock')
class MockSQLTestCaseNoGpdiffNoAnsFile(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @gpdiff False
    """
    sql_dir = 'sql_no_ans/'
    def test_explicit_test_method(self):
        pass
    
@unittest.skip('mock')
class MockSQLTestCaseWithOptimizerOn(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @optimizer_mode on
    """
    db_name=os.getenv('USER')

@unittest.skip('mock')
class MockSQLTestCaseWithOptimizerOff(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @optimizer_mode off
    """
    db_name=os.getenv('USER')

@unittest.skip('mock')
class MockSQLTestCaseWithOptimizerBoth(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @optimizer_mode both
    """
    db_name=os.getenv('USER')
    
class SQLTestCaseTests(unittest.TestCase):
    
    def test_run_sql_test_failure(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCase.test_query02":
                test_case = temp
        self.assertIsNotNone(test_case)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 1)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02.diff')))
        shutil.rmtree(test_case.get_out_dir())
    
    def test_run_sql_test_success(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCase.test_query03":
                test_case = temp
        self.assertIsNotNone(test_case)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        shutil.rmtree(test_case.get_out_dir())
    
    def test_run_entire_sql_test_case(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case = None
        for test_case in test_suite._tests:
            test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_suite.run(test_result)
        # 3 sql files with ans files and 1 explicit method
        self.assertEqual(test_result.testsRun, 4)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 1)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02.diff')))
        shutil.rmtree(test_case.get_out_dir())
    
    def test_verify_setup_teardown(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.

        for test_case in test_suite._tests:
            test_case.__class__.__unittest_skip__ = False

        if os.path.exists(local_path("output/")):
            shutil.rmtree(local_path("output/"))
        test_result = unittest.TestResult()
        test_suite.run(test_result)

        self.assertEqual(test_result.testsRun, 4)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 1)

        # Verify if setup and teardown sqls were executed
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'setup.out')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'setup', 'setup1.out')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'teardown.out')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'teardown', 'teardown1.out')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_setup.out')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_teardown.out')))

    def test_run_explicit_test_method(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCase.test_explicit_test_method":
                test_case = temp
        self.assertIsNotNone(test_case)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)


    def test_with_local_init_file(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCase.test_query04":
                test_case = temp
        self.assertIsNotNone(test_case)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        
    def test_run_no_ans_file(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)

        # Store all test names in a list
        test_case_list = []
        for temp in test_suite._tests:
            test_case_list.append(temp.name)

        # Verify that other sql files with ans files and explicit method is in the list
        self.assertTrue('MockSQLTestCase.test_explicit_test_method' in test_case_list)
        self.assertTrue('MockSQLTestCase.test_query02' in test_case_list)
        # Verify that test_query_no_ans_file is not there, even though the sql file is there without the ans file
        self.assertTrue('MockSQLTestCase.test_query_no_ans_file' not in test_case_list)
        # Verify the default value of generate_ans is no
        self.assertTrue(MockSQLTestCase.generate_ans == 'no')

    def test_gpdiff_no_ans_file(self):
        """
        Test whether we throw an excpetion when there is no ans file for a sql file and if gpdiff is set to True
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseGpdiffNoAnsFile)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCaseGpdiffNoAnsFile.test_query_no_ans_file":
                test_case = temp
        self.assertIsNotNone(test_case)
        
        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 1)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

    def test_no_gpdiff_no_ans_file(self):
        """
        Test whether we construct a test for sqls with no ans files when gpdiff is turned off
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseNoGpdiffNoAnsFile)

        # Store all test names in a list
        test_case_list = []
        for temp in test_suite._tests:
            test_case_list.append(temp.name)

        # Verify that other sql files with ans files and explicit method is in the list
        self.assertTrue('MockSQLTestCaseNoGpdiffNoAnsFile.test_explicit_test_method' in test_case_list)
        self.assertTrue('MockSQLTestCaseNoGpdiffNoAnsFile.test_query02' in test_case_list)
        # Verify that test_query_no_ans_file is there, even though the sql file is there without the ans file
        self.assertTrue('MockSQLTestCaseNoGpdiffNoAnsFile.test_query_no_ans_file' in test_case_list)

    def test_run_generate_ans_file_class_variable(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseGenerateAns)

        # Store all test names in a list
        test_case_list = []
        for temp in test_suite._tests:
            test_case_list.append(temp.name)

        # Verify that other sql files with ans files and explicit method is in the list
        self.assertTrue('MockSQLTestCaseGenerateAns.test_explicit_test_method' in test_case_list)
        self.assertTrue('MockSQLTestCaseGenerateAns.test_query02' in test_case_list)
        # Verify that test_query_no_ans_file is also there, even though its ans file is not there
        self.assertTrue('MockSQLTestCaseGenerateAns.test_query_no_ans_file' in test_case_list)

    def test_run_incorrect_generate_ans_file_class_variable(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseIncorrectGenerateAns)
        count = 0
        for test in test_suite._tests:
            if 'TINCTestCaseLoadFailure' in str(test):
                count += 1
        self.assertEquals(count, 1)
    
    def test_run_sql_generate_ans(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseGenerateAns)

        # Ans file that will be generated
        ans_file = local_path("query_no_ans_file.ans")

        # If ans file is there for some reason, remove it (not testing force here)
        if os.path.exists(ans_file):
            os.remove(ans_file)
        
        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCaseGenerateAns.test_query_no_ans_file":
                test_case = temp
        self.assertIsNotNone(test_case)
        
        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself 
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow 
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

        # Verify that ans file is generated
        self.assertTrue(os.path.exists(local_path("setup.ans")))
        self.assertTrue(os.path.exists(ans_file))
        self.assertTrue(os.path.exists(local_path("teardown.ans")))
        # Cleanup
        os.remove(local_path("setup.ans"))
        os.remove(ans_file)
        os.remove(local_path("teardown.ans"))

    def test_run_sql_force_generate_ans(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseForceGenerateAns)

        # Ans file that will be generated
        ans_file = local_path("query_no_ans_file.ans")

        # Create the empty ans file to allow force to overwrite
        open(ans_file, 'w').close()
        self.assertTrue(os.path.getsize(ans_file) == 0)
        
        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCaseForceGenerateAns.test_query_no_ans_file":
                test_case = temp
        self.assertIsNotNone(test_case)
            
        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself 
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow 
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

        # Verify that ans file is there
        self.assertTrue(os.path.exists(local_path("setup.ans")))
        self.assertTrue(os.path.exists(ans_file))
        self.assertTrue(os.path.exists(local_path("teardown.ans")))
        # Verify that ans file size is greater than 0
        self.assertTrue(os.path.getsize(ans_file) > 0)
        # Cleanup
        os.remove(local_path("setup.ans"))
        os.remove(ans_file)
        os.remove(local_path("teardown.ans"))

    def test_run_sql_force_generate_ans_permission_denied(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseForceGenerateAns)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query04 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCaseForceGenerateAns.test_query04":
                # query04.ans wouldn't be checked-out from perforce, so it would have no write operation allowed
                test_case = temp
        self.assertIsNotNone(test_case)
            
        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself 
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow 
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 1)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

    def test_run_sql_file(self):
        test_case = MockSQLTestCase('test_query03')
        if os.path.exists(test_case.get_out_dir()):
            shutil.rmtree(test_case.get_out_dir())
        # Default mode
        test_case.run_sql_file(local_path('query03.sql'))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03.out')))
        self.assertFalse(self._check_str_in_file('SET optimizer',
                                                os.path.join(test_case.get_out_dir(), 'query03.sql')))

        # Optimizer on mode
        test_case.run_sql_file(local_path('query03.sql'), optimizer=True)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_orca.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_orca.out')))
        self.assertTrue(self._check_str_in_file('SET optimizer=on;',
                                                os.path.join(test_case.get_out_dir(), 'query03_orca.sql')))
                        

        # Optimizer off mode
        test_case.run_sql_file(local_path('query03.sql'), optimizer=False)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_planner.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_planner.out')))
        self.assertTrue(self._check_str_in_file('SET optimizer=off;',
                                                os.path.join(test_case.get_out_dir(), 'query03_planner.sql')))

    def test_run_sql_test_optimizer_on(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseWithOptimizerOn)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCaseWithOptimizerOn.test_query03":
                test_case = temp
        self.assertIsNotNone(test_case)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_orca.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_orca.out')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query03_orca.sql')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query03_orca.out')))
                        
                        
        shutil.rmtree(test_case.get_out_dir())

    def test_run_sql_test_optimizer_off(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseWithOptimizerOff)

        # Find our desired test case in test_suite.
        # This code is a consequence of us only having implemented
        # loadTestsFromTestCase. An implementation of loadTestsFromNames
        # would likely have allowed us to insolate test_query02 directly.
        test_case = None
        for temp in test_suite._tests:
            if temp.name == "MockSQLTestCaseWithOptimizerOff.test_query03":
                test_case = temp
        self.assertIsNotNone(test_case)

        # As explained above, we want MockSQLTestCase to run if and only if
        # it's being invoked by our unit tests. So, it's skipped if discovered
        # directly by unit2. Here, bearing in mind that SQLTestCaseTests is itself
        # triggered by unit2, we override MockSQLTestCase's skip decorator to allow
        # this explicit construction of MockSQLTestCase to proceed.
        test_case.__class__.__unittest_skip__ = False

        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_planner.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_planner.out')))
        self.assertTrue(self._check_str_in_file("SET optimizer=off;",
                                                os.path.join(test_case.get_out_dir(), 'query03_planner.sql')))
        self.assertTrue(self._check_str_in_file("SET optimizer=off;",
                                                os.path.join(test_case.get_out_dir(), 'query03_planner.out')))
                        
                        
        shutil.rmtree(test_case.get_out_dir())

    def test_run_sql_test_optimizer_both(self):
        test_loader = tinctest.TINCTestLoader()
        # For data provider test cases, we have to use loadTestsFromName, since loadTestsFromTestCase won't filter and expand
        test_suite = test_loader.loadTestsFromName("mpp.models.regress.sql_related.regress_sql_test_case.regress_sql_test_case.MockSQLTestCaseWithOptimizerBoth")

        # Find our desired test case in test_suite.
        test_case = None
        new_test_suite = tinctest.TINCTestSuite()
        for temp in test_suite._tests:
            if "MockSQLTestCaseWithOptimizerBoth.test_query03" in temp.name:
                new_test_suite.addTest(temp)
                temp.__class__.__unittest_skip__ = False
                test_case = temp
                
        self.assertIsNotNone(new_test_suite)
        self.assertEquals(new_test_suite.countTestCases(), 2)

        test_result = unittest.TestResult()
        new_test_suite.run(test_result)
        self.assertEqual(test_result.testsRun, 2)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_planner.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_planner.out')))
        self.assertTrue(self._check_str_in_file("SET optimizer=off;",
                                                os.path.join(temp.get_out_dir(), 'query03_planner.sql')))
        self.assertTrue(self._check_str_in_file("SET optimizer=off;",
                                                os.path.join(test_case.get_out_dir(), 'query03_planner.out')))

        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_orca.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query03_orca.out')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query03_orca.sql')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query03_orca.out')))
                        
                        
        shutil.rmtree(test_case.get_out_dir())
        
    def _check_str_in_file(self, check_string, file_path):
        with open(file_path, 'r') as f:
            for line in f:
                if check_string in line:
                    return True
        return False

    def test_run_sql_test_optimizer_minidump_on_failure(self):
        """
        Test whether we gather minidumps on failures when the test is exeucted with optimizer on.
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.regress.sql_related.regress_sql_test_case.' + \
                                                       'regress_sql_test_case.' + \
                                                       'MockSQLTestCaseWithOptimizerOn.test_query02')
        self.assertIsNotNone(test_suite)

        self.assertTrue(len(test_suite._tests), 1)

        test_result = None
        test_case = None
        
        for test in test_suite._tests:
            test.__class__.__unittest_skip__ = False
            test_case = test

        if os.path.exists(test_case.get_out_dir()):
            shutil.rmtree(test_case.get_out_dir())
            
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(test_suite)
            self.assertEqual(test_result.testsRun, 1)
            self.assertEqual(len(test_result.errors), 0)
            self.assertEqual(len(test_result.skipped), 0)
            self.assertEqual(len(test_result.failures), 1)
        
            
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 1)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02_orca.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02_orca.out')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query02_orca.sql')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query02_orca.out')))
        # Verify that we collect minidump on failure for optimizer execution mode
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02_minidump.mdp')))

    @unittest.skip("QAINF-999")
    def test_run_sql_test_optimizer_minidump_on_failure2(self):
        """
        Test whether we gather minidumps on failures when the test is exeucted with optimizer_mode both.
        """
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.regress.sql_related.regress_sql_test_case.' + \
                                                       'regress_sql_test_case.' + \
                                                       'MockSQLTestCaseWithOptimizerBoth.test_query02')
        self.assertIsNotNone(test_suite)
        new_test_suite = tinctest.TINCTestSuite()

        self.assertEquals(test_suite.countTestCases(), 2)

        test_result = None
        test_case = None
        
        for test in test_suite._tests:
            if 'test_query02_orca' in test.name:
                test.__class__.__unittest_skip__ = False
                test_case = test
                new_test_suite.addTest(test)

        self.assertIsNotNone(test_case)
        
        if os.path.exists(test_case.get_out_dir()):
            shutil.rmtree(test_case.get_out_dir())
            
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(new_test_suite)
            self.assertEqual(test_result.testsRun, 1)
            self.assertEqual(len(test_result.errors), 0)
            self.assertEqual(len(test_result.skipped), 0)
            self.assertEqual(len(test_result.failures), 1)
        
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02_orca.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02_orca.out')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query02_orca.sql')))
        self.assertTrue(self._check_str_in_file("SET optimizer=on;",
                                                os.path.join(test_case.get_out_dir(), 'query02_orca.out')))
        # Verify that we collect minidump on failure for optimizer execution mode
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'query02_minidump.mdp')))
        
                        
                        
                
