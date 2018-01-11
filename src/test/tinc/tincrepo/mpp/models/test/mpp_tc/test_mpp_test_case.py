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

from mpp.models.mpp_tc import _MPPMetaClassType
from mpp.models.mpp_tc import MPPDUT
from mpp.models import MPPTestCase

# Need to import hidden models for isinstance verification
from mpp.models.mpp_tc import __gpdbMPPTestCase__
from mpp.models.mpp_tc import __hawqMPPTestCase__

class MockMPPMetaClassTypeGPDB(_MPPMetaClassType):
    """ Mock MPPMetaClassTypeGPDB to reset DUT """
    _MPPMetaClassType.DUT = MPPDUT('gpdb', '1.0.0.0')

@unittest.skip('mock')
class MockMPPTestCaseGPDB(MPPTestCase):
    """ Mock MPPTestCaseGPDB to test MRO and get_version """
    __metaclass__ = MockMPPMetaClassTypeGPDB
    def test_do_stuff(self):
        self.assertTrue(True)

class MockMPPMetaClassTypeHAWQ(_MPPMetaClassType):
    """ Mock MPPMetaClassTypeHAWQ to reset DUT """
    _MPPMetaClassType.DUT = MPPDUT('hawq', '1.1.0.0')

@unittest.skip('mock')
class MockMPPTestCaseHAWQ(MPPTestCase):
    """ Mock MPPTestCaseHAWQ to test MRO and get_version """
    __metaclass__ = MockMPPMetaClassTypeHAWQ
    def test_do_stuff(self):
        self.assertTrue(True)

class MockMPPMetaClassTypeGPDB42(_MPPMetaClassType):
    _MPPMetaClassType.DUT = MPPDUT('gpdb', '4.2')

@unittest.skip('mock')
class MockMPPTestCaseGPDB42(MPPTestCase):
    __metaclass__ = MockMPPMetaClassTypeGPDB42

    def test_do_stuff(self):
        (product, version) = self.get_product_version()
        self.assertEquals(prodcut, 'gpdb')
        self.assertEquals(version, '4.2')



class MPPTestCaseTests(unittest.TestCase):

        
        
    def test_get_product_version(self):
        gpdb_test_case = MockMPPTestCaseGPDB('test_do_stuff')
        self.assertEqual(gpdb_test_case.__class__.__product__, 'gpdb')
        self.assertEqual(gpdb_test_case.__class__.__version_string__, '1.0.0.0')
        self.assertTrue(isinstance(gpdb_test_case, __gpdbMPPTestCase__))
        self.assertFalse(isinstance(gpdb_test_case, __hawqMPPTestCase__))
        
        hawq_test_case = MockMPPTestCaseHAWQ('test_do_stuff')
        self.assertEqual(hawq_test_case.__class__.__product__, 'hawq')
        self.assertEqual(hawq_test_case.__class__.__version_string__, '1.1.0.0')
        self.assertTrue(isinstance(hawq_test_case, __hawqMPPTestCase__))
        self.assertFalse(isinstance(hawq_test_case, __gpdbMPPTestCase__))

@unittest.skip('mock')
class MockMPPTestCaseMetadata(MPPTestCase):

    def test_without_metadata(self):
        self.assertTrue(True)

    def test_with_metadata(self):
        """
        @gather_logs_on_failure True
        @restart_on_fatal_failure True
        @db_name blah
        """
        self.asserTrue(True)

class MPPTestCaseMetadataTests(unittest.TestCase):

    def test_default_metadata(self):
        mpp_test_case = MockMPPTestCaseMetadata('test_without_metadata')
        self.assertFalse(mpp_test_case.gather_logs_on_failure)
        self.assertFalse(mpp_test_case.restart_on_fatal_failure)
    
    def test_with_metadata(self):
        mpp_test_case = MockMPPTestCaseMetadata('test_with_metadata')
        self.assertTrue(mpp_test_case.gather_logs_on_failure)
        self.assertTrue(mpp_test_case.restart_on_fatal_failure)

    def test_out_dir(self):
        self.assertEquals(MockMPPTestCaseMetadata.out_dir, 'output/')
        self.assertEquals(MockMPPTestCaseMetadata.get_out_dir(), os.path.join(os.path.dirname(__file__), 'output/'))

    def test_db_name_metadata(self):
        mpp_test_case = MockMPPTestCaseMetadata('test_with_metadata')
        self.assertEquals(mpp_test_case.db_name, 'blah')
    
    def test_db_name_default(self):
        mpp_test_case = MockMPPTestCaseMetadata('test_without_metadata')
        self.assertEquals(mpp_test_case.db_name, None)

@unittest.skip('mock')
class MockMPPTestCaseGPOPT(MPPTestCase):
    def test_without_metadata(self):
        self.assertTrue(True)
    
    def test_with_metadata_higher(self):
        """
        @gpopt 2.240
        """
        self.asserTrue(True)
    
    def test_with_metadata_lower(self):
        """
        @gpopt 1.0
        """
        self.asserTrue(True)
    
    def test_with_metadata_same(self):
        """
        @gpopt 2.200.1
        """
        self.asserTrue(True)

class MPPTestCaseGPOPTTests(unittest.TestCase):

    def test_without_metadata(self):

        # Test with deployed gpopt version (simulate hawq or gpdb with optimizer)
        MockMPPTestCaseGPOPT.__product_environment__['gpopt'] = "2.200.1"
        mpp_test_case = MockMPPTestCaseGPOPT('test_without_metadata')
        self.assertTrue(mpp_test_case.skip is None)
        
        # Test without deployed gpopt version (simulate gpdb without optimizer)
        MockMPPTestCaseGPOPT.__product_environment__.pop('gpopt', None)
        mpp_test_case = MockMPPTestCaseGPOPT('test_without_metadata')
        self.assertTrue(mpp_test_case.skip is None)
    
    def test_with_metadata_higher(self):

        # Test with deployed gpopt version (simulate hawq or gpdb with optimizer)
        MockMPPTestCaseGPOPT.__product_environment__['gpopt'] = "2.200.1"
        mpp_test_case = MockMPPTestCaseGPOPT('test_with_metadata_higher')
        self.assertTrue(mpp_test_case.skip is not None)
        
        # Test without deployed gpopt version (simulate gpdb without optimizer)
        MockMPPTestCaseGPOPT.__product_environment__.pop('gpopt', None)
        mpp_test_case = MockMPPTestCaseGPOPT('test_with_metadata_higher')
        self.assertTrue(mpp_test_case.skip is not None)
    
    def test_with_metadata_lower(self):

        # Test with deployed gpopt version (simulate hawq or gpdb with optimizer)
        MockMPPTestCaseGPOPT.__product_environment__['gpopt'] = "2.200.1"
        mpp_test_case = MockMPPTestCaseGPOPT('test_with_metadata_lower')
        self.assertTrue(mpp_test_case.skip is None)
        
        # Test without deployed gpopt version (simulate gpdb without optimizer)
        MockMPPTestCaseGPOPT.__product_environment__.pop('gpopt', None)
        mpp_test_case = MockMPPTestCaseGPOPT('test_with_metadata_lower')
        self.assertTrue(mpp_test_case.skip is not None)
    
    def test_with_metadata_same(self):

        # Test with deployed gpopt version (simulate hawq or gpdb with optimizer)
        MockMPPTestCaseGPOPT.__product_environment__['gpopt'] = "2.200.1"
        mpp_test_case = MockMPPTestCaseGPOPT('test_with_metadata_same')
        self.assertTrue(mpp_test_case.skip is None)
        
        # Test without deployed gpopt version (simulate gpdb without optimizer)
        MockMPPTestCaseGPOPT.__product_environment__.pop('gpopt', None)
        mpp_test_case = MockMPPTestCaseGPOPT('test_with_metadata_same')
        self.assertTrue(mpp_test_case.skip is not None)
    
