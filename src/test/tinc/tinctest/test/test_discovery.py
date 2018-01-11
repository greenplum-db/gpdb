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
import re

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import tinctest

from tinctest.discovery import TINCDiscoveryQueryHandler, TINCDiscoveryException

import unittest2 as unittest

@unittest.skip('mock')
class MockTINCTestCase(tinctest.TINCTestCase):
    def test_smoke_01(self):
        """
        
        @tags smoke bug-1
        """
        pass
    def test_functional_02(self):
        """
        
        @tags functional storage
        """
        pass
        
class TINCDiscoveryQueryHandlerTests(unittest.TestCase):

    def test_positive_queries(self):
        test_queries = []
        test_queries.append("class=test_smoke*")
        test_queries.append("module=test_smoke*")
        test_queries.append("method=test_smoke*")
        test_queries.append("package=storage.uao.*")
        test_queries.append("class=* and module=* or package=* and class=SmokeTests*")
        test_queries.append("class=* AND module=* OR package=* AND class=SmokeTests*")
        test_queries.append("class=* and module=* or package=* and class=SmokeTests*")

        test_queries.append("""class != * and module != * or package != * and class != SmokeTests*
         AND class = test""")
        test_queries.append("class != \"test_smoke\"")
        test_queries.append("class != 'test_smoke'")
        test_queries.append("""class = 'test' AND module=\"test\"""")
        test_queries.append("class = test* and method=test*")
        test_queries.append("tags = tag1")

        handler = TINCDiscoveryQueryHandler(test_queries)
        self.assertEquals(len(handler.query_list), len(test_queries))

    def test_negative_queries(self):
        handler = TINCDiscoveryQueryHandler("class=test_smoke*")
        test_queries = []
        test_queries.append("classtest_smoke* and moduletestsmoke*")
        # Partial match should error out
        test_queries.append("class=test_smoke* remaining text")
        # Unsupported operator
        test_queries.append("class=test* XOR package=storage.*")
        test_queries.append("class = test* AND module = test* OR packagetest*")
        # Mixed cases not for operators supported
        test_queries.append("claSS=test* oR Package=storage.*")
        test_queries.append("class=test* and module=test* And package=storage")

        # TBD: Following query currently passes , it should throw an exception instead
        # test_queries.append("class = test*AND module = test*")

        # Unclosed quotes in predicate should error out
        test_queries.append("class='test*")
        # Unmatched quotes in predicate should error out
        test_queries.append("class='test*\"")

        #hanging operators should error out
        test_queries.append("class=test* and method=test* or")
        for query in test_queries:
            try:
                handler = TINCDiscoveryQueryHandler(query)
            except TINCDiscoveryException:
                continue
            self.fail("Query %s did not throw an exception" %query)


    def test_apply_queries_with_single_query(self):
        handler = TINCDiscoveryQueryHandler("method=test_smoke*")
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertTrue(handler.apply_queries(tinc_test_case))

        # metadata equals predicate
        handler = TINCDiscoveryQueryHandler("author=bob")
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertTrue(handler.apply_queries(tinc_test_case))

        # metadata equals predicate with dash (typically used for bugs)
        handler = TINCDiscoveryQueryHandler("tags=bug-1")
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertTrue(handler.apply_queries(tinc_test_case))

        #metadata not equals predicate
        handler = TINCDiscoveryQueryHandler("tags != functional")
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertTrue(handler.apply_queries(tinc_test_case))

        # metadata non-match

        # metadata equals predicate
        handler = TINCDiscoveryQueryHandler("author=alice")
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertFalse(handler.apply_queries(tinc_test_case))

        #metadata not equals predicate
        handler = TINCDiscoveryQueryHandler("tags != smoke")
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertFalse(handler.apply_queries(tinc_test_case))

        # non existing metadata should return False
        handler = TINCDiscoveryQueryHandler("non_existing_tags = smoke")
        # not equals on a non existing metadata will currently return True
        # We will have to decide whether or not to throw an exception for such tests
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertFalse(handler.apply_queries(tinc_test_case))
        
    def test_apply_queries_with_multiple_queries(self):
        queries = []
        queries.append("method=test_smoke*")
        queries.append("class=Mock*")
        queries.append("class='MockTINC*'")
        queries.append("module='test_discovery*'")
        queries.append("package = tinc*.test*")
        queries.append("method != test_smoke_02")
        queries.append("class != NonMock*")
        queries.append("package != storage.uao.*")
        queries.append("module != test_regression*")
        queries.append("author = bob")
        queries.append("tags = smoke")
        queries.append("author != alice")
        queries.append("tags != functional")
        handler = TINCDiscoveryQueryHandler(queries)
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertTrue(handler.apply_queries(tinc_test_case))

        queries = []
        queries.append("method=test_smoke*")
        queries.append("class=Mock*")
        queries.append("class='MockTINC*'")
        queries.append("module='test_discovery*'")
        queries.append("package = storage.uao.*")
        handler = TINCDiscoveryQueryHandler(queries)
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertFalse(handler.apply_queries(tinc_test_case))

    def test_apply_queries_with_multiple_predicates(self):
        queries = []
        queries.append("method=test_smoke* OR class = Mock* or class = MockTINC*")
        queries.append("package=storage.uao.* OR method=test_smoke*")
        queries.append("package=storage.uao.* and method=*testsmoke* and module!=test_discovery or method=test_smoke_01")
        #queries.append("package != storage.uao or method=*testsmoke* and module != test_discovery and class != Mock* and tags = smoke and author != alice")
        handler = TINCDiscoveryQueryHandler(queries)
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertTrue(handler.apply_queries(tinc_test_case))


        queries = []
        queries.append("method=test_smoke* OR class = Mock* or class = MockTINC*")
        queries.append("package=storage.uao.* OR method=test_smoke*")
        queries.append("package=storage.uao.* and method=*testsmoke* and module!=test_discovery or method=test_smoke_01")
        queries.append("package != storage.uao or method=*testsmoke* and module != test_discovery and class != Mock*")
        # Introduce false at the end
        queries.append("package != storage and method != test_smoke_01")
        handler = TINCDiscoveryQueryHandler(queries)
        tinc_test_case = MockTINCTestCase('test_smoke_01')
        self.assertFalse(handler.apply_queries(tinc_test_case))
    
                           

class TINCDiscoveryWithQueriesTests(unittest.TestCase):
    """
    For the following tests, we use the following mock test cases from
    tinctest/test/discovery/mockstorage/uao/test_* and
    tinctest/test/discovery/mockquery/cardinality/test_*

    There are three mock test modules within the two test folders containing 14 tests
    upon which the following querying tests will operate:

    test_functional_*.py: *FunctionalTests with two test methods of the form test_functional_*
    test_smoke_*.py: *SmokaTests* with two tests methods of the form test_smoke_*
    test_uao.py / test_cardinality.py: Three tests methods of the form test_* with tags 'smoke',
                                       'functional', 'stress'
    
    """

    def _discover_and_run_tests(self, start_dirs, patterns, top_level_dir, query_handler):
        tinc_test_loader = tinctest.TINCTestLoader()
        tinc_test_suite = tinc_test_loader.discover(start_dirs = start_dirs,
                                                    patterns = patterns,
                                                    top_level_dir = None,
                                                    query_handler = query_handler
                                                    )
                                                    
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTestResultSet(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            return tinc_test_result
        
        
    def test_discovery_with_no_queries(self):
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = None)
        # Should have run all tests from mockstorage and mockquery
        self.assertEquals(tinc_test_result.testsRun, 14)

    def test_discovery_with_single_predicate(self):
        query_handler = TINCDiscoveryQueryHandler("class=*SmokeTests*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run four smoke tests - 2 each from test_smoke_*.py in mockstorage/uao and mockquery/cardinality
        self.assertEquals(tinc_test_result.testsRun, 4)

        # Predicate on module
        query_handler = TINCDiscoveryQueryHandler("module=*_functional_*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run four functional tests - 2 each from *FunctionalTests in test_functional_*.py in mockstorage/uao and mockquery/cardinality
        self.assertEquals(tinc_test_result.testsRun, 4)

        # Predicate on package
        query_handler = TINCDiscoveryQueryHandler("package=*query.*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run only tests from the package mockquery/*
        self.assertEquals(tinc_test_result.testsRun, 7)


        # predicate on test method
        query_handler = TINCDiscoveryQueryHandler("method=*functional*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run only four functional tests (test_functional_*) from *FunctionalTests in the modules test_functional_*.py in
        # mockquery/cardinality and mockstorage/uao
        self.assertEquals(tinc_test_result.testsRun, 4)

        # predicate on metadtata
        query_handler = TINCDiscoveryQueryHandler("tags = stress")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run the two test methods tagged 'stress' in test_uao.py and test_cardinality.py
        self.assertEquals(tinc_test_result.testsRun, 2)


    def test_single_predicate_not_equals(self):
        query_handler = TINCDiscoveryQueryHandler("class != *SmokeTests*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run everything except the tests in test_smoke_*.py
        self.assertEquals(tinc_test_result.testsRun, 10)

        # Predicate on module
        query_handler = TINCDiscoveryQueryHandler("module != *_functional_*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run everything except the tests in test_functional*.py in mockquery and mockstorage
        self.assertEquals(tinc_test_result.testsRun, 10)

        # Predicate on package
        query_handler = TINCDiscoveryQueryHandler("package != *query.*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run all the tests from within mockstorage
        self.assertEquals(tinc_test_result.testsRun, 7)


        # predicate on test method
        query_handler = TINCDiscoveryQueryHandler("method != *functional*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run all tests in all the modules except for the tests in test_functioanl_*.py of the form test_functional*
        self.assertEquals(tinc_test_result.testsRun, 10)

        # predicate on metadtata
        query_handler = TINCDiscoveryQueryHandler("tags != stress")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)

        # Should have run all the tests except for the tests tagged 'stress' within test_uao.py and test_cardinality.py
        self.assertEquals(tinc_test_result.testsRun, 12)


    def test_multiple_predicates_within_a_query(self):

        # Run all smoke tests
        query_handler = TINCDiscoveryQueryHandler("tags = smoke or method = *smoke* or class = *SmokeTests*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run all the tests from within test_module*.py and one test tagged 'smoke' in test_uao.py and test_cardinality.py
        self.assertEquals(tinc_test_result.testsRun, 6)

        # Run all smoke tests that are not tagged
        query_handler = TINCDiscoveryQueryHandler("method = *smoke* or class = *SmokeTests*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run four tests, two each from within test_smoke_*.py in mockstorage and mockquery
        self.assertEquals(tinc_test_result.testsRun, 4)

        # Run all functional tests
        query_handler = TINCDiscoveryQueryHandler("tags = functional or method = *functional* or class = *FunctionalTests*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run all the functional tests , four from test_functional_*.py and one test each tagged 'functional'
        # from within test_uao.py and test_cardinality.py
        self.assertEquals(tinc_test_result.testsRun, 6)


        # With AND predicates
        query_handler = TINCDiscoveryQueryHandler("method = *functional* and class = *FunctionalTests*")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Should have run only four tests from within test_functional*.py in mockquery and mockstorage
        self.assertEquals(tinc_test_result.testsRun, 4)

        # Run all the tests except for stress tests
        query_handler = TINCDiscoveryQueryHandler("module = test_* and tags != stress")
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Run all the tests from within mockquery and mockstorage except the tests tagged 'stress' within
        # test_uao.py and test_cardinality.py
        self.assertEquals(tinc_test_result.testsRun, 12)


    def test_multiple_queries(self):
        # Run all tests in class *SmokeTests* and starts with test_smoke_*
        queries = ['class = *SmokeTests*', 'method=test_smoke*']
        query_handler = TINCDiscoveryQueryHandler(queries)
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test*'],
                                                        top_level_dir = None, query_handler = query_handler)
        self.assertEquals(tinc_test_result.testsRun, 4)


    def test_queries_with_patterns(self):
        # patterns should be applied first
        queries = ['method = test_*']
        query_handler = TINCDiscoveryQueryHandler(queries)
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery', 'mockquery')
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir], patterns =['test_functional*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Only run two tests within mockquery since the patterns should be applied first and the queries should be applied
        # on the tests that match the pattern
        self.assertEquals(tinc_test_result.testsRun, 2)

    def test_queries_with_patterns_across_multiple_folders(self):
        # patterns should be applied first
        queries = ['method = test_*']
        query_handler = TINCDiscoveryQueryHandler(queries)
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'discovery', 'mockquery')
        test_dir2 = os.path.join(pwd, 'discovery', 'mockstorage')
        
        tinc_test_result = self._discover_and_run_tests(start_dirs = [test_dir, test_dir2], patterns =['test_functional*'],
                                                        top_level_dir = None, query_handler = query_handler)
        # Only run two tests within mockquery since the patterns should be applied first and the queries should be applied
        # on the tests that match the pattern
        self.assertEquals(tinc_test_result.testsRun, 4)

        


    

    


    
        

    
        
        
        
        
        
        

        
        
        
        
        

        
        
