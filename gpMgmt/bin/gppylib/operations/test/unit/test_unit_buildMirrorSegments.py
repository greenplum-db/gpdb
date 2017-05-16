#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

import unittest
import tempfile, os, shutil
from gppylib.commands.base import CommandResult
from gppylib.operations.buildMirrorSegments import GpMirrorListToBuild
from mock import patch, MagicMock, Mock



class buildMirrorSegmentsTestCase(unittest.TestCase):
    def setUp(self):
        self.buildMirrorSegs = GpMirrorListToBuild(
            toBuild = [],
            pool = None, 
            quiet = True, 
            parallelDegree = 0
            )

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster')
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost')
    def test_get_running_postgres_segments_empty_segs(self, mock1, mock2, mock3):
        toBuild = []
        expected_output = []
        segs = self.buildMirrorSegs._get_running_postgres_segments(toBuild)
        self.assertEquals(segs, expected_output)
        
    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', return_value=True)
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', return_value=True)
    def test_get_running_postgres_segments_all_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEquals(segs, mock_segs)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[True, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', return_value=True)
    def test_get_running_postgres_segments_some_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        expected_output.append(mock_segs[0])
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEquals(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[True, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', side_effect=[True, False])
    def test_get_running_postgres_segments_one_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        expected_output.append(mock_segs[0])
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEquals(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[False, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', side_effect=[True, False])
    def test_get_running_postgres_segments_no_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEquals(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[False, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', side_effect=[False, False])
    def test_get_running_postgres_segments_no_pid_running(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEquals(segs, expected_output)

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(rc=0, stdout='/tmp/seg0', stderr='', completed=True, halt=False))
    def test_dereference_remote_symlink_valid_symlink(self, mock1, mock2):
        datadir = '/tmp/link/seg0'
        host = 'h1'
        self.assertEqual(self.buildMirrorSegs.dereference_remote_symlink(datadir, host), '/tmp/seg0')

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(rc=1, stdout='', stderr='', completed=True, halt=False))
    def test_dereference_remote_symlink_unable_to_determine_symlink(self, mock1, mock2):
        datadir = '/tmp/seg0'
        host = 'h1'
        self.assertEqual(self.buildMirrorSegs.dereference_remote_symlink(datadir, host), '/tmp/seg0')

    def test_ensureSharedMemCleaned_no_segments(self):
        self.buildMirrorSegs._GpMirrorListToBuild__ensureSharedMemCleaned(Mock(), [])

    @patch('gppylib.gparray.GpDB.getSegmentHostName', side_effect=['foo1', 'foo2'])
    def test_ensureSharedMemCleaned(self, mock1):
        self.buildMirrorSegs._GpMirrorListToBuild__ensureSharedMemCleaned(Mock(), [Mock(), Mock()])

if __name__ == '__main__':
    unittest.main()
