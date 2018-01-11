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

@unittest.skip('mock')
class MockSQLTestCaseWithSubstitutions(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags tag1
    @db_name mockdb
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    
    def get_substitutions(self):
        substitutions = {'%dbname%': self.db_name,
                         '%tags%': ''.join(self.tags),
                         '%foo%': 'foo'}
        return substitutions

@unittest.skip('mock')
class MockSQLTestCaseWithTemplateSubstitutions(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags tag1
    @db_name mockdb
    """
    sql_dir = 'sql_template/'
    ans_dir = 'expected_template/'
    db_name = 'mockdb'

    template_subs = {'%dbname%': 'mockdb'}
    
    def get_substitutions(self):
        substitutions = {'%tags%': ''.join(self.tags),
                         '%foo%': 'foo'}
        return substitutions
    
class SQLTestCaseSubstitutionTests(unittest.TestCase):

    def test_verify_substitutions(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseWithSubstitutions)
        for test_case in test_suite._tests:
            test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        test_suite.run(test_result)
        self.assertEqual(test_result.testsRun, 3)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)

    def test_verify_substitutions_with_templates(self):
        test_loader = tinctest.TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseWithTemplateSubstitutions)
        for test_case in test_suite._tests:
            test_case.__class__.__unittest_skip__ = False
        test_result = unittest.TestResult()
        test_suite.run(test_result)
        self.assertEqual(test_result.testsRun, 3)
        self.assertEqual(len(test_result.errors), 0)
        self.assertEqual(len(test_result.skipped), 0)
        self.assertEqual(len(test_result.failures), 0)
