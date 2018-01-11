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

from mpp.lib.mpp_tl import MPPTestLib
from mpp.models.mpp_tc import _MPPMetaClassType, MPPDUT

class MockMPPTestLib(MPPTestLib):
    _MPPMetaClassType.DUT = MPPDUT("gpdb", "1.0.0.0")
    pass

class __gpdbMockMPPTestLib__(MockMPPTestLib):
    pass

class MPPTestLibTests(unittest.TestCase):
    def test_dut(self):
        my_test_lib = MockMPPTestLib()
        self.assertTrue(my_test_lib.__product__ is not None)
        self.assertTrue(my_test_lib.__version_string__ is not None)
        self.assertTrue(my_test_lib.__class__.__mro__[0].__name__ is "__gpdbMockMPPTestLib__")
