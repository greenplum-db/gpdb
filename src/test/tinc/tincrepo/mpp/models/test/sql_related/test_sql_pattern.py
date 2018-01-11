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

import inspect
import os

import tinctest

from tinctest.lib import local_path
from tinctest.runner import TINCTestRunner
from tinctest.discovery import TINCDiscoveryQueryHandler

from mpp.models import SQLTestCase, SQLTestCaseException

import unittest2 as unittest

import shutil

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator


class SQLPatternMatchingTests(unittest.TestCase):

    def test_sql_test_case_discovery_with_pattern_matching(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'sql_pattern')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir], patterns = ['sql_pattern.py'],
                                                    top_level_dir = None, query_handler = None)

        test_case = None
        test_result = None
            
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(tinc_test_suite)
            self.assertEqual(test_result.testsRun, 12)
            self.assertEqual(len(test_result.skipped), 12)


    def test_sql_test_case_with_discovery_queries(self):
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'sql_pattern')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir], patterns = ['sql_pattern.py'],
                                                    top_level_dir = None, query_handler = TINCDiscoveryQueryHandler(['method=test_functional_*']))

        test_case = None
        test_result = None
            
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(tinc_test_suite)
            self.assertEqual(test_result.testsRun, 6)
            self.assertEqual(len(test_result.skipped), 6)


        # Queries using metadata from sql files
        tinc_test_loader = tinctest.TINCTestLoader()
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'sql_pattern')
        tinc_test_suite = tinc_test_loader.discover(start_dirs = [test_dir], patterns = ['sql_pattern.py'],
                                                    top_level_dir = None,
                                                    query_handler = TINCDiscoveryQueryHandler(['method=test_functional_* and tags != long']))

        test_case = None
        test_result = None
            
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_runner = TINCTestRunner(stream = buffer, descriptions = True, verbosity = 1)
            test_result = tinc_test_runner.run(tinc_test_suite)
            self.assertEqual(test_result.testsRun, 3)
            self.assertEqual(len(test_result.skipped), 3)
        
        
