#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved. 
#

import unittest
from mock import patch
from gppylib.operations.filespace import is_filespace_configured
import os

class FileSpaceTestCase(unittest.TestCase):
    
    @patch('os.path.exists', return_value=True)
    def test00_is_filespace_configured(self, mock_obj):
        self.assertEqual(is_filespace_configured(), True)

    @patch('os.path.exists', return_value=False)
    def test02_is_filespace_configured(self, mock_obj):
        self.assertEqual(is_filespace_configured(), False)

