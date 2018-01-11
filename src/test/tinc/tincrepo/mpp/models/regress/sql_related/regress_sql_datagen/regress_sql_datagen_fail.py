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
import unittest2 as unittest
import tinctest
from tinctest.lib import local_path

from mpp.lib.datagen import TINCTestDatabase
from mpp.lib.PSQL import PSQL
from mpp.models import SQLTestCase

from gppylib.commands.base import Command, CommandResult

#@unittest.skip('mock')
class MockDbFailTestCase(SQLTestCase):
    """
    
    @db_name test_db_fail
    @description database setup failure
    """
    sql_dir = 'fail_sql/'
    ans_dir = 'fail_expected/'

#@unittest.skip('mock')
class MockDbTestCase(SQLTestCase):
    """
    
    @db_name test_db
    @description database setup 
    """
    sql_dir = 'fail_sql/'
    ans_dir = 'fail_expected/'

class MockDataSetTestCase(SQLTestCase):
    """
    
    @db_name dataset_0.01_heap
    @description dataset database setup 
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'


#class SQLDatagenFailTest(unittest.TestCase):
#
#    def test_datagen_fail(self):
#        test_loader = tinctest.TINCTestLoader()
#        test_suite = test_loader.loadTestsFromTestCase(MockDbFailTestCase)
#        for test_case in test_suite._tests:
#            print test_case
#            test_case.__class__.__unittest_skip__ = False
#            test_result = unittest.TestResult()
#            test_case.run(test_result)
#

class TestDbFailSetup(TINCTestDatabase):
    """
    Create a test database but fail during setup
    @db_name test_db_fail
    """

    def __init__(self, database_name = "test_db_fail"):
        super(TestDbFailSetup, self).__init__(database_name)

    def setUp(self):
        # Call out to TINCTestDatabase.setUp which will return True if the database already exists
        if super(TestDbFailSetup, self).setUp():
            return True

        # run some sqls to setup the database
        ret = PSQL.run_sql_file(local_path('fail_db_setup1.sql'), dbname = self.db_name)

        return True

class TestDbSetup(TINCTestDatabase):
    """
    Create a test database but fail during setup
    @db_name test_db
    """

    def __init__(self, database_name = "test_db"):
        super(TestDbSetup, self).__init__(database_name)

    def setUp(self):
        # Call out to TINCTestDatabase.setUp which will return True if the database already exists
        if super(TestDbSetup, self).setUp():
            return True

        # run some sqls to setup the database
        ret = PSQL.run_sql_file(local_path('fail_db_setup.sql'), dbname = self.db_name)

        return True
