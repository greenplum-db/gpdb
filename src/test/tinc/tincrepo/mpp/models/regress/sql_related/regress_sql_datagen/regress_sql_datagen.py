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

from mpp.models import SQLTestCase

from mpp.lib.datagen import TINCTestDatabase
import unittest2 as unittest

import shutil

# we're testing SQLTestCase as it pertains to tinc.py (and only tinc.py)
# as such, any attempts by raw unit2 to discover and load MockSQLTestCase must be averted
@unittest.skip('mock')
class MockSQLTestCase(SQLTestCase):
    """
    
    @db_name testdb
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    def test_explicit_test_method(self):
        pass

class SQLTestCaseTests(unittest.TestCase):

    def test_datagen(self):
        test_case = MockSQLTestCase('test_query02')
        self.assertIsNotNone(test_case)
        test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        test_case.run(test_result)
        self.assertEqual(test_case.db_name, 'testdb')
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)


# This is the TINCTestCustomDatabase for the user who wants to define their own
# Database class to create specific database, you must provide the metadata
# @db_name same as @db_name in your TestCase. If you don't provide this, the
# program will use the default TINCTestDatabase to create the database.
class TINCTestCustomDatabase(TINCTestDatabase):
    """
    Database for MockSQLTestCase tests
    @db_name testdb
    """

    def __init__(self, database_name = "testdb"):
        super(TINCTestCustomDatabase, self).__init__(database_name)

    def setUp(self):
        # Call out to TINCTestDatabase.setUp which will return True if the database already exists
        if super(TINCTestCustomDatabase, self).setUp():
            return True

