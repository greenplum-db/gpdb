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

import os
import shutil

from tinctest.lib import local_path, Gpdiff

from mpp.models import SQLTestCase

@unittest.skip('mock')
class MockSQLTestCase(SQLTestCase):
    def test_foo(self):
        pass

@unittest.skip('mock')
class MockSQLTestCaseWithGucs(SQLTestCase):
    '''
    @gucs gp_optimizer=on;gp_log_optimizer=on
    '''

class SQLTestCaseTests(unittest.TestCase):
    test_case = MockSQLTestCase('test_foo')

    def setUp(self):
        if os.path.exists(SQLTestCaseTests.test_case.get_out_dir()):
            shutil.rmtree(SQLTestCaseTests.test_case.get_out_dir())
            
    def test_pos(self):
        test_case = MockSQLTestCase('test_test_pos')
        test_case.run_test()
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'test_pos.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'test_pos.out')))
                
    def test_neg(self):
        test_case = MockSQLTestCase('test_test_neg')
        with self.assertRaises(AssertionError) as cm:
            result = test_case.run_test()
        
    def test_with_gucs(self):
        test_case = MockSQLTestCaseWithGucs('test_test_pos')
        test_case.run_test()
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'test_pos.sql')))
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'test_pos.out')))
        self.assertTrue(Gpdiff.are_files_equal(
                        os.path.join(test_case.get_out_dir(), 'test_pos.sql'),
                        local_path('gucs/test_pos_gucs.sql')))
        
    def test_file_with_gucs(self):
        test_case = MockSQLTestCase('test_test_file_with_gucs')
        result = test_case.run_test()
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'test_file_with_gucs.sql')))
        self. _check_gucs_exist_in_file(os.path.join(test_case.get_out_dir(), 'test_file_with_gucs.sql'), test_case.gucs)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(), 'test_file_with_gucs.out')))
        self.assertTrue(Gpdiff.are_files_equal(
                        os.path.join(test_case.get_out_dir(), 'test_file_with_gucs.sql'),
                        local_path('gucs/test_file_with_gucs.sql')))
        
    def _check_gucs_exist_in_file(self, sql_file, gucs):
        with open(sql_file, 'r') as f:
            for guc in gucs:
                guc_exists = False
                for line in f:
                    if guc in line:
                        guc_exists = True
                        break
                self.assertTrue(guc_exists)

    def test_gather_minidump(self):
        test_case = MockSQLTestCase('test_test_md')
        test_case.gather_mini_dump(test_case.sql_file)
        self.assertTrue(os.path.exists(os.path.join(test_case.get_out_dir(),
                                                    os.path.basename(test_case.sql_file).replace('.sql', '_minidump.mdp'))))
        

