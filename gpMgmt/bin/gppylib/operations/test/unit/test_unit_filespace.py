#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved. 
#

import hashlib
import os
import shutil
import tempfile
import unittest

from gppylib.gparray import GpDB, GpArray
from gppylib.operations.filespace import is_filespace_configured, MoveTransFilespaceLocally, UpdateFlatFiles
from mock import Mock, patch
from gppylib.test.unit.gp_unittest import GpTestCase


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

class UpdateFlatFilesTestCase(GpTestCase):
    def setUp(self):
        self.subject = UpdateFlatFiles(None, None, None)
        self.subject.logger = Mock(
            spec=['warning', 'debug'])
        self.apply_patches([
            patch('gppylib.operations.filespace.logger', return_value=Mock(spec=['debug'])),
            patch('os.path.exists'),
        ])
        self.mock_path_exists = self.get_mock_from_apply_patch('exists')

    @patch('gppylib.operations.filespace.ParallelOperation.run')
    @patch('gppylib.operations.filespace.GetFilespaceEntries.run',
           return_value=[(16385L, 1, '/tmp/filespace/m/gpseg-1'),
                         (16385L, 4, '/tmp/filespace/m1/gpseg0'),
                         (16385L, 2, '/tmp/filespace/p1/gpseg0'),
                         (16385L, 5, '/tmp/filespace/m2/gpseg1'),
                         (16385L, 3, '/tmp/filespace/p2/gpseg1')
                         ])
    def test_update_flat_file_logs_warning_for_down_segments(self, mk1, mk2):
        self.mock_path_exists.return_value = True
        self.subject.gparray = GpArray([GpDB.initFromString("1|-1|p|p|s|u|c448b39a33aa|mdw|5432|5532|/home/gpadmin/data/master/gpseg-1||"),
                                        GpDB.initFromString("2|0|p|p|s|u|sdw1|sdw1|6000|8000|/home/gpadmin/data/primary/gpseg0||"),
                                        GpDB.initFromString("3|1|p|p|c|u|sdw2|sdw2|6000|8000|/home/gpadmin/data/primary/gpseg1||"),
                                        GpDB.initFromString("4|0|m|m|s|u|sdw2|sdw2|7000|9000|/home/gpadmin/data/mirror/gpseg0||"),
                                        GpDB.initFromString("5|1|m|m|s|d|sdw1|sdw1|7000|9000|/home/gpadmin/data/mirror/gpseg1||")])
        self.subject.execute()
        self.subject.logger.warning.assert_any_call('Segment with DBID 5 on host sdw1 is down, skipping updating the temporary filespace entries.')