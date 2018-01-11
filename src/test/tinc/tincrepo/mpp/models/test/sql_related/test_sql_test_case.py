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
import sys
import shutil

from contextlib import closing
from datetime import datetime
from StringIO import StringIO

import unittest2 as unittest
from unittest2.runner import _WritelnDecorator

from tinctest import TINCTestLoader
from tinctest import TINCTextTestResult

from mpp.models.mpp_tc import _MPPMetaClassType
from mpp.models.mpp_tc import MPPDUT

from mpp.models import SQLTestCase, SQLTestCaseException

# Since we overwrite optimizer_mode depending on product/version, force the internal variables to gpdb/4.3
# This will ensure that optimizer_mode both works as designed, and all the tests written for that works.
# _MPPMetaClassType.DUT = MPPDUT('gpdb', '4.3')

@unittest.skip('mock')
class MockSQLTestCase(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @gucs gp_optimizer=on;gp_log_optimizer=on
    @optimizer_mode ON
    """
    def setUp(self):
        pass
    def test_explicit_definition(self):
        pass

@unittest.skip('mock')
class MockSQLTemplateTestCase(SQLTestCase):
    template_dir = 'template_dir'
    template_subs = {'%PERCENTAGE%' : 'my_percent',
                     '&AMP&' : 'my_amp',
                     '@AT' : 'my_at'}
    
@unittest.skip('mock')
class MockSQLTemplateTestCaseExplicit(SQLTestCase):
    template_dir = 'template_dir'
    template_subs = {'%PERCENTAGE%' : 'my_percent',
                     '&AMP&' : 'my_amp',
                     '@AT' : 'my_at'}
    
@unittest.skip('mock')
class MockSQLTemplateTestCaseRegular(SQLTestCase):
    template_dir = 'template_dir'
    template_subs = {'%PERCENTAGE%' : 'my_percent',
                     '&AMP&' : 'my_amp',
                     '@AT' : 'my_at'}

class MockMPPMetaClassTypeGPDB43(_MPPMetaClassType):
    _MPPMetaClassType.DUT = MPPDUT('gpdb', '4.3')
    
@unittest.skip('mock')
class MockSQLTestCaseForOptimizerMode(SQLTestCase):
    """
    
    @description test case with metadata
    @created 2012-07-05 12:00:00
    @modified 2012-07-05 12:00:02
    @tags orca hashagg
    @gucs gp_optimizer=on;gp_log_optimizer=on
    @optimizer_mode on
    """
    __metaclass__ = MockMPPMetaClassTypeGPDB43
    pass

@unittest.skip('mock')
class MockSQLTestCaseForOptimizerModeBoth(SQLTestCase):
    """
    @optimizer_mode both
    """
    __metaclass__ = MockMPPMetaClassTypeGPDB43
    pass

@unittest.skip('mock')
class MockSQLTestCaseInvalidOptimizerMode(SQLTestCase):
    """
    @optimizer_mode invalid_value
    """
    __metaclass__ = MockMPPMetaClassTypeGPDB43
    pass

class MockMPPMetaClassTypeHAWQ(_MPPMetaClassType):
    _MPPMetaClassType.DUT = MPPDUT('hawq', '1.1.0.0')

@unittest.skip('mock')
class MockSQLTestCaseOptimizerModeHAWQ(SQLTestCase):
    __metaclass__ = MockMPPMetaClassTypeHAWQ

    def test_optimizer_mode_both(self):
        """
        @optimizer_mode both
        """
        pass

    def test_optimizer_mode_on(self):
        """
        @optimizer_mode on
        """
        pass

    def test_optimizer_mode_off(self):
        """
        @optimizer_mode off
        """
        pass

class SQLTestCaseTests(unittest.TestCase):

    def test_infer_metadata(self):
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)
        test_case = None
        for case in test_suite._tests:
            if case.name == "MockSQLTestCase.test_query02":
                test_case = case
        self.assertNotEqual(test_case, None)
        self.assertEqual(test_case.name, "MockSQLTestCase.test_query02")
        self.assertEqual(test_case.author, 'kumara64')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))
        self.assertEqual(test_case.gucs, set(['gp_optimizer=on', 'gp_log_optimizer=on']))
        
    def test_optimizer_mode_from_sql_file(self):
        test_case = MockSQLTestCaseForOptimizerMode('test_query02')
        # sql file query02.sql has overriden optimizer_mode
        self.assertEqual(test_case.optimizer_mode, 'off')

    def test_optimizer_mode_from_class(self):
        test_case = MockSQLTestCaseForOptimizerMode('test_query03')
        self.assertEqual(test_case.optimizer_mode, 'on')

    def test_optimizer_mode_invalid_value(self):
        with self.assertRaises(SQLTestCaseException) as cm:
            test_case = MockSQLTestCaseInvalidOptimizerMode('test_query01')
                
    def test_direct_instantiation(self):
        test_case = MockSQLTestCase('test_query02')
        self.assertEqual(test_case.name, "MockSQLTestCase.test_query02")
        self.assertEqual(test_case.author, 'kumara64')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-08 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))

    def test_explicit_test_fixtures(self):
        test_case = MockSQLTestCase('test_explicit_definition')
        self.assertEqual(test_case.name, "MockSQLTestCase.test_explicit_definition")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test case with metadata')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-05 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-05 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg']))

    def test_explicit_test_fixtures_through_loading(self):
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCase)
        # 4 tests for 3 sqls in the directory and 1 explicit test method
        self.assertEqual(test_suite.countTestCases(), 4)

    def test_optimizer_mode_both(self):
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseForOptimizerModeBoth)
        for test in test_suite._tests:
            # Data provider should exists for query01 and query03.
            # query02 shouldn't have it, since its optimizer mode is overwritten with value 'off'
            if test.name == "MockSQLTestCaseForOptimizerModeBoth.test_query01" or test.name == "MockSQLTestCaseForOptimizerModeBoth.test_query03":
                self.assertEqual(test.optimizer_mode, "both")
                self.assertEqual(test.data_provider, "optimizer_handling")
            else:
                self.assertNotEqual(test.optimizer_mode, "both")
                self.assertTrue(test.data_provider is None)

    def test_optimizer_mode_hawq(self):
        """
        Test whether optimizer_mode both is overriden in hawq to None
        """
        test_case = MockSQLTestCaseOptimizerModeHAWQ('test_optimizer_mode_both')
        self.assertIsNone(test_case.optimizer_mode)
        test_case = MockSQLTestCaseOptimizerModeHAWQ('test_optimizer_mode_on')
        self.assertEquals(test_case.optimizer_mode, 'on')
        test_case = MockSQLTestCaseOptimizerModeHAWQ('test_optimizer_mode_off')
        self.assertEquals(test_case.optimizer_mode, 'off')
        
    
class MockSQLTestCaseForSkip(SQLTestCase):
    """
    
    @description test case to test skip tag
    @created 2012-08-07 12:00:00
    @modified 2012-08-07 12:00:02
    """

class SQLTestCaseSkipTests(unittest.TestCase):
    def test_skip_tag_in_sql_file(self):
        test_case = MockSQLTestCaseForSkip('test_query01')
        self.assertEqual(test_case.name, "MockSQLTestCaseForSkip.test_query01")
        self.assertEqual(test_case.skip, 'demonstrating skipping')
    def test_skip_when_tag_in_sql_file(self):
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromTestCase(MockSQLTestCaseForSkip)
        test_case = None
        for case in test_suite._tests:
            if case.name == "MockSQLTestCaseForSkip.test_query01":
                test_case = case
        self.assertNotEqual(test_case, None)
        self.assertEqual(test_case.name, "MockSQLTestCaseForSkip.test_query01")
        with closing(_WritelnDecorator(StringIO())) as buffer:
            test_result = TINCTextTestResult(buffer, True, 1)
            test_case.run(test_result)
        self.assertEqual(test_result.testsRun, 1)
        self.assertEqual(len(test_result.failures), 0)
        self.assertEqual(len(test_result.skipped), 1)
        self.assertEqual(len(test_result.errors), 0)

@unittest.skip('mock')
class MockSQLTestCaseForLoader(SQLTestCase):
    @classmethod
    def setUpClass(cls):
        pass

class SQLTestLoaderTests(unittest.TestCase):
    def test_load_implicit_python_from_name(self):
        """Test loadTestsFromName for a dynamically generated sql test method"""
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.test.sql_related.test_sql_test_case.MockSQLTestCaseForLoader.test_query01')
        test_case = test_suite._tests[0]
        self.assertEqual(test_case.name, "MockSQLTestCaseForLoader.test_query01")
        self.assertEqual(test_case.author, 'lammin')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-20 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-20 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))

    def test_load_test_from_class_name(self):
        """Test loadTestsFromName for a class name"""
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.test.sql_related.test_sql_test_case.MockSQLTestCaseForLoader')
        test_case = None
        for my_test_case in test_suite._tests:
            if my_test_case.name == 'MockSQLTestCaseForLoader.test_query01':
                test_case = my_test_case
                break

        self.assertTrue(test_case is not None)
        self.assertEqual(test_case.name, "MockSQLTestCaseForLoader.test_query01")
        self.assertEqual(test_case.author, 'lammin')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-20 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-20 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))

    def test_load_test_from_class_name_with_supplementary_sqls(self):
        """Test loadTestsFromName for a class name"""
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.test.sql_related.test_sql_test_case.MockSQLTestCaseForLoader')
        # 3 tests for 3 sql tests in the current directory. 
        self.assertEquals(len(test_suite._tests), 3)
        for test_case in test_suite._tests:
            if test_case.name == 'MockSQLTestCaseForLoader.test_query03':
                break

        self.assertEqual(test_case.name, "MockSQLTestCaseForLoader.test_query03")
        self.assertEqual(test_case.author, 'balasr3')
        self.assertEqual(test_case.description, 'test sql test case sql')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-20 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-20 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))

class SQLTemplateTests(unittest.TestCase):
    def test_templates_regular_sql(self):
        """Test loadTestsFromName for a dynamically generated sql test method."""
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.test.sql_related.test_sql_test_case.MockSQLTemplateTestCaseRegular.test_query01')
        test_case = test_suite._tests[0]
        # Non-template test case should work as is...
        self.assertEqual(test_case.name, "MockSQLTemplateTestCaseRegular.test_query01")
        self.assertEqual(test_case.author, 'lammin')
        self.assertEqual(test_case.description, 'test sql test case')
        self.assertEqual(test_case.created_datetime, datetime.strptime('2012-07-20 12:00:00', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.modified_datetime, datetime.strptime('2012-07-20 12:00:02', '%Y-%m-%d %H:%M:%S'))
        self.assertEqual(test_case.tags, set(['orca', 'hashagg', 'executor']))
    
    def test_templates_template_sql_file(self):
        """Test loadTestsFromName for a dynamically generated sql template test method."""
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.test.sql_related.test_sql_test_case.MockSQLTemplateTestCaseExplicit.test_template_query04')
        test_case = test_suite._tests[0]
        # Template test case should work as if it is non-template test case...
        self.assertEqual(test_case.name, "MockSQLTemplateTestCaseExplicit.test_template_query04")
        self.assertEqual(test_case.author, 'shahn17')
        self.assertEqual(test_case.description, 'template test case')

        sql_file_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCaseExplicit", "template_query04.sql")
        ans_file_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCaseExplicit", "template_query04.ans")
        original_sql_file_path = os.path.join(os.path.dirname(sys.modules[test_case.__class__.__module__].__file__), test_case.__class__.sql_dir, test_case.__class__.template_dir, "query04.sql")
        original_ans_file_path = os.path.join(os.path.dirname(sys.modules[test_case.__class__.__module__].__file__), test_case.__class__.ans_dir, test_case.__class__.template_dir, "query04.ans")
        self.assertEqual(test_case.sql_file, sql_file_path)
        self.assertEqual(test_case.ans_file, ans_file_path)
        self.assertEqual(test_case._original_sql_file, original_sql_file_path)
        self.assertEqual(test_case._original_ans_file, original_ans_file_path)
        self.assertTrue(os.path.exists(test_case.sql_file))
        self.assertTrue(os.path.exists(test_case.ans_file))
        self.assertTrue(os.path.exists(test_case._original_sql_file))
        self.assertTrue(os.path.exists(test_case._original_ans_file))
        # Cleanup
        dir_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCaseExplicit")
        self.assertTrue(os.path.exists(dir_path))
        shutil.rmtree(dir_path)
    
    def test_templates_all_files(self):
        """Test loadTestsFromName for a class name"""
        test_loader = TINCTestLoader()
        test_suite = test_loader.loadTestsFromName('mpp.models.test.sql_related.test_sql_test_case.MockSQLTemplateTestCase')
        # 5 tests for 3 sql files in the current directory, and 2 sql files in the template directory
        self.assertEquals(len(test_suite._tests), 5)
        for test_case in test_suite._tests:
            if test_case.name == 'MockSQLTemplateTestCase.test_template_query04':
                break
        
        self.assertEqual(test_case.name, "MockSQLTemplateTestCase.test_template_query04")
        self.assertEqual(test_case.author, 'shahn17')
        self.assertEqual(test_case.description, 'template test case')
        
        sql_file_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCase", "template_query04.sql")
        ans_file_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCase", "template_query04.ans")
        original_sql_file_path = os.path.join(os.path.dirname(sys.modules[test_case.__class__.__module__].__file__), test_case.__class__.sql_dir, test_case.__class__.template_dir, "query04.sql")
        original_ans_file_path = os.path.join(os.path.dirname(sys.modules[test_case.__class__.__module__].__file__), test_case.__class__.ans_dir, test_case.__class__.template_dir, "query04.ans")
        self.assertEqual(test_case.sql_file, sql_file_path)
        self.assertEqual(test_case.ans_file, ans_file_path)
        self.assertEqual(test_case._original_sql_file, original_sql_file_path)
        self.assertEqual(test_case._original_ans_file, original_ans_file_path)
        self.assertTrue(os.path.exists(test_case.sql_file))
        self.assertTrue(os.path.exists(test_case.ans_file))
        self.assertTrue(os.path.exists(test_case._original_sql_file))
        self.assertTrue(os.path.exists(test_case._original_ans_file))
        
        # Template test case sql file should exists
        sql_file_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCase", "template_query04.sql")
        self.assertTrue(os.path.exists(sql_file_path))
        sql_file_data = None
        with open(sql_file_path, 'r') as sql_file_object:
            sql_file_data = sql_file_object.read()
        self.assertTrue(sql_file_data is not None)
        # Correct substitution
        self.assertTrue('my_percent' in sql_file_data)
        # Error in python code
        self.assertTrue('my_at@' in sql_file_data)
        # Error in sql template
        self.assertTrue('&AMP' in sql_file_data)

        # Template test case ans file should exists
        ans_file_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCase", "template_query05.ans")
        self.assertTrue(os.path.exists(ans_file_path))
        ans_file_data = None
        with open(ans_file_path, 'r') as sql_file_object:
            ans_file_data = sql_file_object.read()
        self.assertTrue(ans_file_data is not None)
        # Correct substitution
        self.assertTrue('my_percent' in ans_file_data)
        # Error in python code
        self.assertTrue('my_at@' in ans_file_data)
        # Error in ans template
        self.assertTrue('&AMP' in ans_file_data)

        # Cleanup
        dir_path = os.path.join(test_case.get_out_dir(), "MockSQLTemplateTestCase")
        self.assertTrue(os.path.exists(dir_path))
        shutil.rmtree(dir_path)

@unittest.skip('mock')
class MockTINCTestCaseForLoaderDiscovery(SQLTestCase):
    def test_lacking_product_version(self):
        """
        
        @maintainer balasr3
        @description test stuff
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags storage
        """
        pass
    def test_containing_product_version(self):
        """
        
        @maintainer balasr3
        @description test stuff
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags storage
        @product_version gpdb: 4.2
        """
        pass
    def test_main_product_version(self):
        """
        
        @maintainer balasr3
        @description test stuff
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags storage
        @product_version gpdb: main
        """
        pass

    def test_containing_product_version_exclusive_range(self):
        """
        
        @maintainer balasr3
        @description test stuff
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags storage
        @product_version gpdb: (4.1.0.0-main)
        """
        pass

    def test_containing_product_version_inclusive_range(self):
        """
        
        @maintainer balasr3
        @description test stuff
        @created 2012-07-05 12:00:00
        @modified 2012-07-05 12:00:02
        @tags storage
        @product_version gpdb: [4.2.0.0-main]
        """
        pass


class TINCTestLoaderDiscoveryTests(unittest.TestCase):
    def test_matching_author(self):
        test_case = MockTINCTestCaseForLoaderDiscovery('test_lacking_product_version')
        self.assertTrue(test_case.match_metadata("author", "pedroc"))
        self.assertFalse(test_case.match_metadata("author", "kumara64"))
    def test_matching_maintainer(self):
        test_case = MockTINCTestCaseForLoaderDiscovery('test_lacking_product_version')
        self.assertTrue(test_case.match_metadata("maintainer", "balasr3"))
        self.assertFalse(test_case.match_metadata("maintainer", "kumara64"))
    def test_matching_tags(self):
        test_case = MockTINCTestCaseForLoaderDiscovery('test_lacking_product_version')
        self.assertTrue(test_case.match_metadata("tags", "storage"))
        self.assertFalse(test_case.match_metadata("tags", "text_analytics"))
