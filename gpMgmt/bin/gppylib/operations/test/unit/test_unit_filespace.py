#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved. 
#

import unittest
from mock import patch
from gppylib.operations.filespace import is_filespace_configured, MoveTransFilespaceLocally
import os
import tempfile
import shutil
import hashlib

class FileSpaceTestCase(unittest.TestCase):
    def setUp(self):
        self.subject = MoveTransFilespaceLocally(None, None, None, None, None)
        self.one_dir = tempfile.mkdtemp()
        self.one_file = tempfile.mkstemp(dir=self.one_dir)

    def tearDown(self):
        if self.one_dir and os.path.exists(self.one_dir):
            shutil.rmtree(self.one_dir)

    @patch('os.path.exists', return_value=True)
    def test00_is_filespace_configured(self, mock_obj):
        self.assertEqual(is_filespace_configured(), True)

    @patch('os.path.exists', return_value=False)
    def test02_is_filespace_configured(self, mock_obj):
        self.assertEqual(is_filespace_configured(), False)

    def test_move_trans_filespace_locally(self):
        with open(self.one_file[1], 'w') as f:
            f.write("some text goes here")
        m = hashlib.sha256()
        m.update("some text goes here")
        local_digest = m.hexdigest()
        test_digest = self.subject.get_sha256(self.one_dir)
        self.assertEquals(test_digest, local_digest)