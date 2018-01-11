#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
from gppylib.gparray import Segment
from gppylib.test.unit.gp_unittest import *
import tempfile, os, shutil
from gppylib.commands.base import CommandResult
from mock import patch, MagicMock, Mock
from gppylib.operations.buildMirrorSegments import GpMirrorListToBuild
from gppylib.operations.startSegments import StartSegmentsResult


class buildMirrorSegmentsTestCase(GpTestCase):
    def setUp(self):
        self.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
        self.apply_patches([
        ])

        self.buildMirrorSegs = GpMirrorListToBuild(
            toBuild = [],
            pool = None, 
            quiet = True, 
            parallelDegree = 0,
            logger=self.logger
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
        self.logger.warning.assert_any_call('Unable to determine if /tmp/seg0 is symlink. Assuming it is not symlink')

    def test_ensureSharedMemCleaned_no_segments(self):
        self.buildMirrorSegs._GpMirrorListToBuild__ensureSharedMemCleaned(Mock(), [])
        self.assertEquals(self.logger.call_count, 0)

    @patch('gppylib.operations.utils.ParallelOperation.run')
    @patch('gppylib.gparray.Segment.getSegmentHostName', side_effect=['foo1', 'foo2'])
    def test_ensureSharedMemCleaned(self, mock1, mock2):
        self.buildMirrorSegs._GpMirrorListToBuild__ensureSharedMemCleaned(Mock(), [Mock(), Mock()])
        self.logger.info.assert_any_call('Ensuring that shared memory is cleaned up for stopped segments')
        self.assertEquals(self.logger.warning.call_count, 0)

    @patch('gppylib.operations.buildMirrorSegments.read_era')
    @patch('gppylib.operations.startSegments.StartSegmentsOperation')
    def test_startAll_succeeds(self, mock1, mock2):
        result = StartSegmentsResult()
        result.getFailedSegmentObjs()
        mock1.return_value.startSegments.return_value = result
        result = self.buildMirrorSegs._GpMirrorListToBuild__startAll(Mock(), [Mock(), Mock()], [])
        self.assertTrue(result)

    @patch('gppylib.operations.buildMirrorSegments.read_era')
    @patch('gppylib.operations.startSegments.StartSegmentsOperation')
    def test_startAll_fails(self, mock1, mock2):
        result = StartSegmentsResult()
        failed_segment = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        result.addFailure(failed_segment, 'reason', 'reasoncode')
        mock1.return_value.startSegments.return_value = result
        result = self.buildMirrorSegs._GpMirrorListToBuild__startAll(Mock(), [Mock(), Mock()], [])
        self.assertFalse(result)
        self.logger.warn.assert_any_call('Failed to start segment.  The fault prober will shortly mark it as down. '
                                         'Segment: sdw1:/data/primary0:content=0:dbid=2:mode=s:status=u: REASON: reason')

if __name__ == '__main__':
    run_tests()
