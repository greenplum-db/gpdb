"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

from contextlib import closing
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import tinctest
from tinctest import TINCTestSuite
from tinctest import TINCTestLoader

from tinctest.case import _TINCProductVersion, TINCInvalidProductVersionException
from tinctest.case import _TINCProductVersionRange, _TINCProductVersionMetadata

import unittest2 as unittest

@unittest.skip('Mock test case')
class MockTINCTestCaseWithProductVersionMetadata(tinctest.TINCTestCase):
    def test_with_no_product_version(self):
        self.assertTrue(True)
    
    def test_with_one_product_version(self):
        """
        @product_version test: 1.0.0.0
        """
        self.assertTrue(True)

    def test_with_multiple_product_version(self):
        """
        @product_version test: [1.0.0.0-1.1.0.0] , test2: (1.0.0.0-1.1.0.0)
        """
        self.assertTrue(True)

class TINCTestCaseWithProductVersionMetadataTests(unittest.TestCase):
    def test_with_no_product_version(self):
        tinc_test_case = MockTINCTestCaseWithProductVersionMetadata('test_with_no_product_version')
        self.assertIsNone(tinc_test_case.product_version)
       
    def test_with_one_product_version(self):
        tinc_test_case = MockTINCTestCaseWithProductVersionMetadata('test_with_one_product_version')
        self.assertIsNotNone(tinc_test_case.product_version)
        self.assertEquals(len(tinc_test_case.product_version.product_version_included), 1)
        self.assertTrue('test' in tinc_test_case.product_version.product_version_included)
        self.assertEquals(tinc_test_case.product_version.product_version_included['test'][0], _TINCProductVersion('1.0.0.0'))

    def test_with_multiple_product_version(self):
        tinc_test_case = MockTINCTestCaseWithProductVersionMetadata('test_with_multiple_product_version')
        self.assertIsNotNone(tinc_test_case.product_version)
        self.assertEquals(len(tinc_test_case.product_version.product_version_included), 2)
        self.assertTrue('test' in tinc_test_case.product_version.product_version_included)
        self.assertEquals(tinc_test_case.product_version.product_version_included['test'][0], _TINCProductVersionRange("[1.0.0.0-1.1.0.0]"))
        self.assertTrue('test2' in tinc_test_case.product_version.product_version_included)
        self.assertEquals(tinc_test_case.product_version.product_version_included['test2'][0], _TINCProductVersionRange('(1.0.0.0 - 1.1.0.0)'))

@unittest.skip('Mock test case')
class MockTINCTestCaseWithGetVersion(tinctest.TINCTestCase):
    def get_product_version(self):
        product = 'test'
        version = '1.1.0.0'
        return (product, version)

    def test_should_run_if_no_product_version(self):
        self.assertTrue(True)

    def test_should_run_for_exact_match(self):
        """
        @product_version test: 1.1.0.0
        """
        self.assertTrue(True)

    def test_should_not_run_if_no_product_specified(self):
        """
        @product_version some_other_product: 1.2.0.0
        """
        self.assertTrue(True)

    def test_should_run_when_multiple_products_specified(self):
        """
        @product_version test2: 1.2.0.0 , test: 1.1.0.0
        """
        self.assertTrue(True)

    def test_should_run_within_matching_inclusive_range(self):
        """
        @product_version test : [1.0.0.0 - 1.2.0.0]
        """
        self.assertTrue(True)

    def test_should_run_within_matching_exclusive_range(self):
        """
        @product_version test: (1.0.0.0 - 1.2.0.0)
        """
        self.assertTrue(True)

    def test_should_run_within_matching_border_inclusive_start_range(self):
        """
        @product_version test: [1.1.0.0-1.2.0.0)
        """
        self.assertTrue(True)

    def test_should_run_within_matching_border_inclusive_end_range(self):
        """
        @product_version test: (1.0.0.0-1.1.0.0]
        """
        self.assertTrue(True)

    def test_should_skip_with_exact_matching(self):
        """
        @product_version test: 1.2.0.0
        """
        self.assertTrue(True)

    def test_should_skip_with_exclusive_start_range(self):
        """
        @product_version test: (1.1.0.0- 2.0.0.0]
        """
        self.assertTrue(True)

    def test_should_skip_with_exclusive_end_range(self):
        """
        @product_version test: [1.0.0.0- 1.1.0.0)
        """
        self.assertTrue(True)

    def test_should_run_with_empty_version_string(self):
        """
        @product_version test2: 1.0.0.0, test:
        """
        self.assertTrue(True)

@unittest.skip('Mock test case')
class MockTINCTestCaseWithGetProductVersion(tinctest.TINCTestCase):

    def get_product_version(self):
        return ('gpdb', '4.2')

    def test_should_not_run(self):
        """
        @product_version gpdb: [4.2.7.2-4.2.9.9]
        """
        self.assertTrue(True)

        
        
class TINCTestCaseProductVersionTests(unittest.TestCase):

    def run_test(self, method_name, skip_expected,
                 test_case_prefix = 'tinctest.test.test_version_check.MockTINCTestCaseWithGetVersion'):
        loader = TINCTestLoader()
        tinc_test_suite = loader.loadTestsFromName('%s.%s' %(test_case_prefix, method_name), dryrun=True)
        with closing(_WritelnDecorator(StringIO())) as buffer:
            tinc_test_result = tinctest.TINCTextTestResult(buffer, True, 1)
            tinc_test_suite.run(tinc_test_result)
            text = buffer.getvalue()
        self.assertEqual(tinc_test_result.testsRun, 1)
        self.assertEqual(len(tinc_test_result.failures), 0)
        if skip_expected:
            self.assertEqual(len(tinc_test_result.skipped), 1)
        else:
            self.assertEqual(len(tinc_test_result.skipped), 0)
        self.assertEqual(len(tinc_test_result.errors), 0)
        
    def test_should_run_if_no_product_version(self):
        self.run_test('test_should_run_if_no_product_version', False)

    def test_should_run_for_exact_match(self):
        self.run_test('test_should_run_for_exact_match', False)

    def test_should_not_run_if_no_product_specified(self):
        self.run_test('test_should_not_run_if_no_product_specified', True)

    def test_should_run_within_matching_inclusive_range(self):
        self.run_test('test_should_run_within_matching_inclusive_range', False)

    def test_should_run_within_matching_exclusive_range(self):
        self.run_test('test_should_run_within_matching_exclusive_range', False)

    def test_should_run_within_matching_border_inclusive_start_range(self):
        self.run_test('test_should_run_within_matching_border_inclusive_start_range', False)

    def test_should_run_within_matching_border_inclusive_end_range(self):
        self.run_test('test_should_run_within_matching_border_inclusive_end_range', False)

    def test_should_skip_with_exact_matching(self):
        self.run_test('test_should_skip_with_exact_matching', True)

    def test_should_skip_with_exclusive_start_range(self):
        self.run_test('test_should_skip_with_exclusive_start_range', True)

    def test_should_skip_with_exclusive_end_range(self):
        self.run_test('test_should_skip_with_exclusive_end_range', True)

    def test_should_run_with_empty_version_string(self):
        self.run_test('test_should_run_with_empty_version_string', False)

    def test_should_not_run_product_version_default_filler(self):
        """
        Checks whether product version is extended using upper bound by default.
        For eg: , a version string like 4.2 should be expanded to 4.2.99.99 by default
        when constructing the version object for the deployed version
        """
        self.run_test('test_should_not_run', True, test_case_prefix='tinctest.test.test_version_check.MockTINCTestCaseWithGetProductVersion')
        

class TINCProductVersionMetadataTests(unittest.TestCase):

    def test_invalid_product_version_metadata(self):
        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            m = _TINCProductVersionMetadata("testtest")

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # No comma in ranges
            m = _TINCProductVersionMetadata("test: [1.0.0.0,1.1.1.0]")

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # No invalid brackets
            m = _TINCProductVersionMetadata("test: {1.0.0.0,1.1.1.0}")

            # No hotfixes within ranges

    def test_add(self):
        
        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-],  hawq: 4.3")
        op2 = _TINCProductVersionMetadata("gpdb: 4.3")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], 4.3, hawq: 4.3"))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("gpdb: 4.3")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], 4.3"))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("gpdb: 4.2")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]"))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("gpdb: [4.3-]")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]"))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("hawq: [4.3-], 4.2")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], hawq: [4.3-], 4.2"))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], -4.3")
        op2 = _TINCProductVersionMetadata("hawq: [4.3-], 4.2, -4.1.x")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], hawq: [4.3-], 4.2, gpdb: -4.3, hawq: -4.1.x"))
                
        op1 = _TINCProductVersionMetadata("gpdb: ")
        op2 = _TINCProductVersionMetadata("hawq:")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: , hawq: "))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2.3")
        op2 = _TINCProductVersionMetadata("hawq:, gpdb: ")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: , gpdb: 4.2.3, hawq: "))

        op1 = _TINCProductVersionMetadata("gpdb: [4.2.3-]")
        op2 = _TINCProductVersionMetadata("hawq:, gpdb: [4.2-]")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.2.3-], [4.2-], hawq: "))

        op1 = _TINCProductVersionMetadata("gpdb: [4.2.3-]")
        op2 = _TINCProductVersionMetadata("hawq:, gpdb: [4.2.x.x-]")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.2.3-], [4.2.x.x-], hawq: "))

        #HotFix can be added in range
        op1 = _TINCProductVersionMetadata("gpdb: [4.3.1.0LA1-]")
        op2 = _TINCProductVersionMetadata("gpdb: 4.2")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.3.1.0LA1-], 4.2"))
        
         
    def test_sub(self):
        
        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-],  hawq: 4.3")
        op2 = _TINCProductVersionMetadata("gpdb: 4.2")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.3-], hawq: 4.3"))

        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("gpdb: 4.3")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]"))

        
        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("gpdb: 4.2")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.3-]"))

        
        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("gpdb: [4.3-]")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2"))

    
        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]")
        op2 = _TINCProductVersionMetadata("hawq: [4.3-], 4.2")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-]"))

        
        op1 = _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], -4.3, hawq: 4.2, -4.1.x")
        op2 = _TINCProductVersionMetadata("hawq: [4.3-], 4.2, -4.1.x")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: 4.2, [4.3-], gpdb: -4.3"))

        op1 = _TINCProductVersionMetadata("gpdb: ")
        op2 = _TINCProductVersionMetadata("hawq:")
        result = op1 - op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: "))

        """
        op1 = _TINCProductVersionMetadata("gpdb: 4.2.3")
        op2 = _TINCProductVersionMetadata("hawq:, gpdb: ")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: , gpdb: 4.2.3, hawq: "))

        op1 = _TINCProductVersionMetadata("gpdb: [4.2.3-]")
        op2 = _TINCProductVersionMetadata("hawq:, gpdb: [4.2-]")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.2.3-], [4.2-], hawq: "))

        op1 = _TINCProductVersionMetadata("gpdb: [4.2.3-]")
        op2 = _TINCProductVersionMetadata("hawq:, gpdb: [4.2.x.x-]")
        result = op1 + op2
        self.assertTrue( result == _TINCProductVersionMetadata("gpdb: [4.2.3-], [4.2.x.x-], hawq: "))
        """
          
        #Covering HotFix in metadata
        op1 = _TINCProductVersionMetadata("gpdb: [4.3.1.1-], 4.3.1.0LA1")
        self.assertTrue( op1.match_product_version("gpdb", "4.3.1.0LA1"))


            
class TINCProductVersionTests(unittest.TestCase):

    def test_empty_string(self):
        v = _TINCProductVersion('')
        self.assertEquals(v.version, ['x'] * 4)
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('x')
        self.assertEquals(v.version, ['x'] * 4)
        self.assertIsNone(v.hotfix)

    def test_filler(self):
        # Default filler
        v = _TINCProductVersion('4.2.x')
        self.assertEquals(v.version, ['4', '2', 'x', 'x'])
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('4.2')
        self.assertEquals(v.version, ['4', '2', 'x', 'x'])
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('4.2.x.x')
        self.assertEquals(v.version, ['4', '2', 'x', 'x'])
        self.assertIsNone(v.hotfix)

        # Provide filler string
        v = _TINCProductVersion('4.2.x', filler='0')
        self.assertEquals(v.version, ['4', '2', 'x', '0'])
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('4.2', filler='0')
        self.assertEquals(v.version, ['4', '2', '0', '0'])
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('4.2.x.x', filler='0')
        self.assertEquals(v.version, ['4', '2', 'x', 'x'])
        self.assertIsNone(v.hotfix)

    def test_hotfix(self):
        v = _TINCProductVersion('4.2.1.0MS1')
        self.assertEquals(v.version, ['4', '2', '1', '0'])
        self.assertEquals(v.hotfix, 'MS1')
        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            v = _TINCProductVersion('4.2.x.xMS1')
        
    def test_invalid_version_strings(self):
        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            v = _TINCProductVersion('4')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Hotfix can be given only with four part version
            v = _TINCProductVersion('4.2MS1')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Anything greater than 4 part should throw an exception
            v = _TINCProductVersion('4.2.1.1.1')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Anything other than digits or 'x' should fail
            v = _TINCProductVersion('4.2.a.1')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Just a string should fail
            v = _TINCProductVersion('test.test.test.test')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Anything beyond hotfix should fail
            v = _TINCProductVersion('4.2.1.3MS1_DEV')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Anything other than x or a digit anywhere should fail
            v = _TINCProductVersion('4.2.y.3MS1')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Anything other than x or a digit anywhere should fail
            v = _TINCProductVersion('4.2.y.zMS1')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Anything other than x or a digit anywhere should fail
            v = _TINCProductVersion('4.2.xx.1MS1')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            # Invalid filler string
            v = _TINCProductVersion('4.2MS1', filler='y')

    def test_main_version(self):
        v = _TINCProductVersion('main')
        self.assertEquals(v.version, ['99', '99', '99', '99'])
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('MAIN')
        self.assertEquals(v.version, ['99', '99', '99', '99'])
        self.assertIsNone(v.hotfix)
        v = _TINCProductVersion('mAin')
        self.assertEquals(v.version, ['99', '99', '99', '99'])
        self.assertIsNone(v.hotfix)

    def test_equality(self):
        # Equality operations
        self.assertEquals(_TINCProductVersion('4.2.1.0'), _TINCProductVersion('4.2.1.0'))
        self.assertEquals(_TINCProductVersion('x'), _TINCProductVersion('4.2.1.0'))
        self.assertEquals(_TINCProductVersion('x'), _TINCProductVersion('x'))
        self.assertEquals(_TINCProductVersion('4.2.1'), _TINCProductVersion('4.2.1.0'))
        self.assertEquals(_TINCProductVersion('4.2.1'), _TINCProductVersion('4.2.1.99'))
        self.assertEquals(_TINCProductVersion('4.2'), _TINCProductVersion('4.2.1.99'))
        self.assertEquals(_TINCProductVersion('4.x'), _TINCProductVersion('4.2.1.99'))
        self.assertEquals(_TINCProductVersion(''), _TINCProductVersion('4.2.1.99'))
        self.assertEquals(_TINCProductVersion(None), _TINCProductVersion('4.2.1.99'))
        self.assertEquals(_TINCProductVersion(''), _TINCProductVersion('4.2.1.99MS1'))
        self.assertEquals(_TINCProductVersion('4.2'), _TINCProductVersion('4.2.1.99MS1'))
        self.assertEquals(_TINCProductVersion('4.2.x'), _TINCProductVersion('4.2.1.99MS1'))
        self.assertEquals(_TINCProductVersion('4.2.x.99'), _TINCProductVersion('4.2.1.99MS1'))

        # Comparison operations
        self.assertGreater(_TINCProductVersion('4.2.0.0'), _TINCProductVersion('4.1'))
        self.assertGreater(_TINCProductVersion('4.2.0.0'), _TINCProductVersion('4.1.x'))
        self.assertGreater(_TINCProductVersion('4.2.0.0'), _TINCProductVersion('4.1.x.x'))
        self.assertGreater(_TINCProductVersion('4.2.0.0'), _TINCProductVersion('4.1.99.99'))
        self.assertGreater(_TINCProductVersion('4.2.0.0'), _TINCProductVersion('4.1.99.99MS1'))
        self.assertGreater(_TINCProductVersion('main'), _TINCProductVersion('5.10'))
        self.assertGreater(_TINCProductVersion('4.2.2.1A1B1'), _TINCProductVersion('4.2.2.1'))
        self.assertGreater(_TINCProductVersion('4.2.0.0MS1'), _TINCProductVersion('4.2', filler='0'))
        self.assertGreater(_TINCProductVersion('4.2.7', filler='99'), _TINCProductVersion('4.2.7.0MS1'))


    def test_increment(self):
        v = _TINCProductVersion('4.2.1.1').incr()
        self.assertEquals(v, _TINCProductVersion('4.2.1.2'))
        v = _TINCProductVersion('').incr()
        self.assertEquals(v, _TINCProductVersion('x.x.x.x'))
        v = _TINCProductVersion('4.2.x').incr()
        self.assertEquals(v, _TINCProductVersion('4.2.x.x'))
        v = _TINCProductVersion('4.x').incr()
        self.assertEquals(v, _TINCProductVersion('4.x.x.x'))
        v = _TINCProductVersion('x').incr()
        self.assertEquals(v, _TINCProductVersion('x.x.x.x'))
        v = _TINCProductVersion(None).incr()
        self.assertEquals(v, _TINCProductVersion('x.x.x.x'))
        v = _TINCProductVersion('main').incr()
        self.assertEquals(v, _TINCProductVersion('main'))
        #Modified according to the Hotfix support in the version range
        v = _TINCProductVersion('4.2.1.1MS1').incr()
        self.assertEquals(v, _TINCProductVersion('4.2.1.1MS2'))

        v = _TINCProductVersion('4.2.1.99').incr()
        self.assertEquals(v, _TINCProductVersion('4.2.2.0'))
        v = _TINCProductVersion('4.2.99.1').incr()
        self.assertEquals(v, _TINCProductVersion('4.2.99.2'))
        v = _TINCProductVersion('4.99.99.1').incr()
        self.assertEquals(v, _TINCProductVersion('4.99.99.2'))
        v = _TINCProductVersion('99.99.99.1').incr()
        self.assertEquals(v, _TINCProductVersion('99.99.99.2'))
        v = _TINCProductVersion('4.2.99.99').incr()
        self.assertEquals(v, _TINCProductVersion('4.3.0.0'))
        v = _TINCProductVersion('4.99.99.99').incr()
        self.assertEquals(v, _TINCProductVersion('5.0.0.0'))
        v = _TINCProductVersion('99.99.99.99').incr()
        self.assertEquals(v, _TINCProductVersion('99.99.99.99'))

    def test_decrement(self):
        v = _TINCProductVersion('4.2.1.1').decr()
        self.assertEquals(v, _TINCProductVersion('4.2.1.0'))
        v = _TINCProductVersion('').decr()
        self.assertEquals(v, _TINCProductVersion('x.x.x.x'))
        v = _TINCProductVersion('4.2.x').decr()
        self.assertEquals(v, _TINCProductVersion('4.2.x.x'))
        v = _TINCProductVersion('4.x').decr()
        self.assertEquals(v, _TINCProductVersion('4.x.x.x'))
        v = _TINCProductVersion('x').decr()
        self.assertEquals(v, _TINCProductVersion('x.x.x.x'))
        v = _TINCProductVersion(None).decr()
        self.assertEquals(v, _TINCProductVersion('x.x.x.x'))
        v = _TINCProductVersion('main').decr()
        self.assertEquals(v, _TINCProductVersion('99.99.99.98'))
        #Modified according to the Hotfix support in the version range
        v = _TINCProductVersion('4.2.1.1MS1').decr()
        self.assertEquals(v, _TINCProductVersion('4.2.1.1'))

        v = _TINCProductVersion('4.2.1.0').decr()
        self.assertEquals(v, _TINCProductVersion('4.2.0.99'))
        v = _TINCProductVersion('4.2.0.1').decr()
        self.assertEquals(v, _TINCProductVersion('4.2.0.0'))
        v = _TINCProductVersion('4.0.0.1').decr()
        self.assertEquals(v, _TINCProductVersion('4.0.0.0'))
        v = _TINCProductVersion('0.0.0.1').decr()
        self.assertEquals(v, _TINCProductVersion('0.0.0.0'))
        v = _TINCProductVersion('4.2.0.0').decr()
        self.assertEquals(v, _TINCProductVersion('4.1.99.99'))
        v = _TINCProductVersion('4.0.0.0').decr()
        self.assertEquals(v, _TINCProductVersion('3.99.99.99'))
        v = _TINCProductVersion('0.0.0.0').decr()
        self.assertEquals(v, _TINCProductVersion('0.0.0.0'))
        
class TINCProductVersionRangeTests(unittest.TestCase):

    def test_version_range(self):
        vr = _TINCProductVersionRange('[4.2.1.0-4.2.3.0]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.1.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.2.3.0'))

        #HotFix Version range: Lowerbound check
        vr = _TINCProductVersionRange('[4.3.1.0LA1-]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.3.1.0LA1'))

        #HotFix Version range: Upperbound check
        vr = _TINCProductVersionRange('[-4.3.1.0LA1]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.1.0LA1'))
        
    def test_lower_bound_filler(self):
        vr = _TINCProductVersionRange('[4.2-4.2.3.0]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.0.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.2.3.0'))

        vr = _TINCProductVersionRange('[-4.2.3.0]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('0.0.0.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.2.3.0'))

        #HotFix lower bound

        vr = _TINCProductVersionRange('[4.3.1.0LA1-]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))
        self.assertTrue(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.3.1.0LA1'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('main'))

        vr = _TINCProductVersionRange('(4.3.1.0LA1-)')
        self.assertFalse(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))
        self.assertFalse(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.3.1.0LA2'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('99.99.99.98'))


    def test_upper_bound_filler(self):
        vr = _TINCProductVersionRange('[4.2-4.3]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.0.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.99.99'))

        vr = _TINCProductVersionRange('[4.2-]')
        self.assertTrue(vr.lower_bound_inclusive and vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.0.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('main'))

        #HotFix upper bound filler    
        vr = _TINCProductVersionRange('[-4.3.1.0LA1]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))
        self.assertTrue(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('0.0.0.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.1.0LA1'))

    def test_exclusive_brackets(self):
        vr = _TINCProductVersionRange('(4.2.1.1-4.3)')
        self.assertFalse(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.1.2'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.99.98'))

        vr = _TINCProductVersionRange('(4.2.1.1-4.3]')
        self.assertFalse(vr.lower_bound_inclusive)
        self.assertTrue(vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.1.2'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.99.99'))

        vr = _TINCProductVersionRange('[4.2.1.1-4.3)')
        self.assertTrue(vr.lower_bound_inclusive)
        self.assertFalse(vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.2.1.1'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.99.98'))

        vr = _TINCProductVersionRange('(4.2.99.99-4.4.0.0)')
        self.assertFalse(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.3.0.0'))
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.99.99'))

        #HotFix Version range exclusive bound check
        vr = _TINCProductVersionRange('(4.3.1.0LA1-)')
        self.assertFalse(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.lower_bound_version, _TINCProductVersion('4.3.1.0LA2'))

        vr = _TINCProductVersionRange('(-4.3.1.0LA1)')
        self.assertFalse(vr.lower_bound_inclusive or vr.upper_bound_inclusive)
        self.assertEquals(vr.upper_bound_version, _TINCProductVersion('4.3.1.0'))
        self.assertFalse(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))
        
        
    def test_invalid_range(self):
        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            vr = _TINCProductVersionRange('test')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            vr = _TINCProductVersionRange('4.2-4.3')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            vr = _TINCProductVersionRange('[4.2-4.3')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            vr = _TINCProductVersionRange('[4.2]')

        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            vr = _TINCProductVersionRange('[4.2-4.3]test')

        # Lower bound > upper bound
        with self.assertRaises(TINCInvalidProductVersionException) as cm:
            vr = _TINCProductVersionRange('[4.2-4.1]')

    def test_match_version(self):
        vr = _TINCProductVersionRange('[4.2-4.4]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.0.0')))
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.99.99')))
        self.assertTrue(vr.match_version(_TINCProductVersion('4.2.0.0')))
        self.assertTrue(vr.match_version(_TINCProductVersion('4.4.99.99')))

        """
        These cases are not expected. We will always compare with a four part version.
        self.assertTrue(vr.match_version(_TINCProductVersion('4.2')))
        self.assertTrue(vr.match_version(_TINCProductVersion('4.2.1.x')))
        self.assertFalse(vr.match_version(_TINCProductVersion('4.x')))
        """

        # Hotfix matches
        self.assertTrue(vr.match_version(_TINCProductVersion('4.2.0.0MS1')))
        # A hotfix version is considered greater than the actual version
        self.assertFalse(vr.match_version(_TINCProductVersion('4.4.99.99MS1')))

        vr = _TINCProductVersionRange('[4.2.6.0-9.99.99.99]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.0.0EFT')))
        
        #Testing new range for the HOTFIX (4.3.1.0LA1)"
        vr = _TINCProductVersionRange('[4.3.1.0-]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))

        #Testing Version range with HotFix with lowerBound
        vr = _TINCProductVersionRange('[4.3.1.0LA1-]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))

        #Testing Version range with HotFix with upperBound
        vr = _TINCProductVersionRange('[-4.3.1.0LA1]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.1.0LA1')))

        #Testing Version range with HotFix
        vr = _TINCProductVersionRange('[4.3.1.0LA1-]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.1.0LA2')))

        #Negative test case for version range with hotfix
        vr = _TINCProductVersionRange('[-4.3.1.0LA1]')
        self.assertFalse(vr.match_version(_TINCProductVersion('4.3.1.0LA2')))

        #Hotfix range check with other hotfix version
        vr = _TINCProductVersionRange('[-4.3.0.1MA1]')
        self.assertTrue(vr.match_version(_TINCProductVersion('4.3.0.1LA2')))
