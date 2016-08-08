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

import unittest2 as unittest

from mpp.models.mpp_tc import _MPPMetaClassType
from mpp.models.mpp_tc import MPPDUT

from mpp.models import SQLTestCase, SQLTestCaseException
from mpp.models.sql_tc import __gpdbSQLTestCase__, __hawqSQLTestCase__

class MockMPPMetaClassTypeGPDB42(_MPPMetaClassType):
    _MPPMetaClassType.DUT = MPPDUT('gpdb', '4.2')

@unittest.skip('mock')
class MockSQLTestCaseGPDB42(SQLTestCase):
    __metaclass__ = MockMPPMetaClassTypeGPDB42

    def test_do_stuff(self):
        (product, version) = self.get_product_version()
        self.assertEquals(prodcut, 'gpdb')
        self.assertEquals(version, '4.2')

class SQLTestCaseGPDB42Tests(unittest.TestCase):

    def test_get_optimizer_mode_42(self):
        """
        Test whether get_optimizer_mode returns None for versions < 43
        """
        gpdb_test_case = MockSQLTestCaseGPDB42('test_do_stuff')
        self.assertEqual(gpdb_test_case.__class__.__product__, 'gpdb')
        self.assertEqual(gpdb_test_case.__class__.__version_string__, '4.2')
        self.assertTrue(isinstance(gpdb_test_case, __gpdbSQLTestCase__))
        self.assertFalse(isinstance(gpdb_test_case, __hawqSQLTestCase__))
        self.assertIsNone(MockSQLTestCaseGPDB42.get_global_optimizer_mode())
