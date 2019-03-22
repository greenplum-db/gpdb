#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
from gppylib.gparray import Segment, GpArray
from gppylib.test.unit.gp_unittest import *
import logging
import os
import shutil
import StringIO
import tempfile
from gppylib.commands import base
from mock import patch, MagicMock, Mock
from gppylib.operations.buildMirrorSegments import GpMirrorListToBuild
from gppylib.operations.startSegments import StartSegmentsResult


class buildMirrorSegmentsTestCase(GpTestCase):
    def setUp(self):
        self.master = Segment(content=-1, preferred_role='p', dbid=1, role='p', mode='s',
                              status='u', hostname='masterhost', address='masterhost-1',
                              port=1111, datadir='/masterdir')

        self.primary = Segment(content=0, preferred_role='p', dbid=2, role='p', mode='s',
                               status='u', hostname='primaryhost', address='primaryhost-1',
                               port=3333, datadir='/primary')

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
    @patch('gppylib.commands.base.Command.get_results', return_value=base.CommandResult(rc=0, stdout='/tmp/seg0', stderr='', completed=True, halt=False))
    def test_dereference_remote_symlink_valid_symlink(self, mock1, mock2):
        datadir = '/tmp/link/seg0'
        host = 'h1'
        self.assertEqual(self.buildMirrorSegs.dereference_remote_symlink(datadir, host), '/tmp/seg0')

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=base.CommandResult(rc=1, stdout='', stderr='', completed=True, halt=False))
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
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        result.addFailure(failed_segment, 'reason', 'reasoncode')
        mock1.return_value.startSegments.return_value = result
        result = self.buildMirrorSegs._GpMirrorListToBuild__startAll(Mock(), [Mock(), Mock()], [])
        self.assertFalse(result)
        self.logger.warn.assert_any_call('Failed to start segment.  The fault prober will shortly mark it as down. '
                                         'Segment: sdw1:/data/primary0:content=0:dbid=2:role=p:preferred_role=p:mode=s:status=u: REASON: reason')

    def _createGpArrayWith2Primary2Mirrors(self):
        self.master = Segment.initFromString(
                "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")

        return GpArray([self.master, self.primary0, self.primary1, mirror0, mirror1])

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_ports_differ(self):
        self.master.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.master.port = 1111
        self.primary.port = 1111

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Segment dbid's 2 and 1 on host samehost cannot have the same port 1111"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_data_directories_differ(self):
        self.master.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.master.datadir = "/data"
        self.primary.datadir = "/data"

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Segment dbid's 2 and 1 on host samehost cannot have the same data directory '/data'"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

class SegmentProgressTestCase(GpTestCase):
    """
    Test case for GpMirrorListToBuild._join_and_show_segment_progress().
    """
    def setUp(self):
        self.pool = Mock(spec=base.WorkerPool)
        self.buildMirrorSegs = GpMirrorListToBuild(
            toBuild=[],
            pool=self.pool,
            quiet=True,
            parallelDegree=0,
            logger=Mock(spec=logging.Logger)
        )

    def test_command_output_is_displayed_once_after_worker_pool_completes(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2
        cmd.get_results.return_value.stdout = "string 1\n"

        cmd2 = Mock(spec=base.Command)
        cmd2.remoteHost = 'host2'
        cmd2.dbid = 4
        cmd2.get_results.return_value.stdout = "string 2\n"

        outfile = StringIO.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd, cmd2], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\n'
            'host2 (dbid 4): string 2\n'
        ))

    def test_command_output_is_displayed_once_for_every_blocked_join(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2

        cmd.get_results.side_effect = [Mock(stdout="string 1"), Mock(stdout="string 2")]

        outfile = StringIO.StringIO()
        self.pool.join.side_effect = [False, True]
        self.buildMirrorSegs._join_and_show_segment_progress([cmd], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\n'
            'localhost (dbid 2): string 2\n'
        ))

    def test_inplace_display_uses_ansi_escapes_to_overwrite_previous_output(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2
        cmd.get_results.side_effect = [Mock(stdout="string 1"), Mock(stdout="string 2")]

        cmd2 = Mock(spec=base.Command)
        cmd2.remoteHost = 'host2'
        cmd2.dbid = 4
        cmd2.get_results.side_effect = [Mock(stdout="string 3"), Mock(stdout="string 4")]

        outfile = StringIO.StringIO()
        self.pool.join.side_effect = [False, True]
        self.buildMirrorSegs._join_and_show_segment_progress([cmd, cmd2], inplace=True, outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\x1b[K\n'
            'host2 (dbid 4): string 3\x1b[K\n'
            '\x1b[2A'
            'localhost (dbid 2): string 2\x1b[K\n'
            'host2 (dbid 4): string 4\x1b[K\n'
        ))

    def test_errors_during_command_execution_are_displayed(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2
        cmd.get_results.return_value.stderr = "some error\n"
        cmd.run.side_effect = base.ExecutionError("Some exception", cmd)

        cmd2 = Mock(spec=base.Command)
        cmd2.remoteHost = 'host2'
        cmd2.dbid = 4
        cmd2.get_results.return_value.stderr = ''
        cmd2.run.side_effect = base.ExecutionError("Some exception", cmd2)

        outfile = StringIO.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd, cmd2], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): some error\n'
            'host2 (dbid 4): \n'
        ))

if __name__ == '__main__':
    run_tests()
