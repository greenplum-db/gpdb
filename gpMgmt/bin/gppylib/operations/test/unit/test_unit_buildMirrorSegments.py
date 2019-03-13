#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
from gppylib.gparray import GpArray, GpDB
from gppylib.test.unit.gp_unittest import *
import tempfile, os, shutil
from gppylib.commands.base import CommandResult
from mock import patch, MagicMock, Mock
from gppylib.operations.buildMirrorSegments import GpMirrorListToBuild
from gppylib.operations.startSegments import StartSegmentsResult


class buildMirrorSegmentsTestCase(GpTestCase):
    def setUp(self):
        self.master = GpDB(content=-1, preferred_role='p', dbid=1, role='p', mode='s',
                           status='u', hostname='masterhost', address='masterhost-1',
                           port=1111, datadir='/masterdir', replicationPort=2222)

        self.primary = GpDB(content=0, preferred_role='p', dbid=2, role='p', mode='s',
                            status='u', hostname='primaryhost', address='primaryhost-1',
                            port=3333, datadir='/primary', replicationPort=4444)

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
    @patch('gppylib.gparray.GpDB.getSegmentHostName', side_effect=['foo1', 'foo2'])
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
        failed_segment = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        result.addFailure(failed_segment, 'reason', 'reasoncode')
        mock1.return_value.startSegments.return_value = result
        result = self.buildMirrorSegs._GpMirrorListToBuild__startAll(Mock(), [Mock(), Mock()], [])
        self.assertFalse(result)
        self.logger.warn.assert_any_call('Failed to start segment.  The fault prober will shortly mark it as down. '
                                         'Segment: sdw1:/data/primary0:content=0:dbid=2:mode=s:status=u: REASON: reason')

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_ports_differ(self):
        self.master.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.master.port = 1111
        self.primary.port = 1111

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Segment dbid's 1 and 2 on host samehost cannot have the same port 1111"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_QE_replication_ports_differ(self):
        self.master.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.master.replicationPort = 2222
        self.primary.replicationPort = 2222

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Segment dbid's 1 and 2 on host samehost cannot have the same replication port 2222"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

    def test_checkForPortAndDirectoryConflicts__checks_QE_replication_port_is_set(self):
        self.primary.replicationPort = None

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Replication port is not set for segment dbid 2 on host primaryhost"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_both_QE_replication_port_and_port_differ(self):
        self.master.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.primary.port = 3333
        self.primary.replicationPort = 3333

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Segment dbid 2 on host samehost cannot have the same value 3333 for port and replication port"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_data_directories_differ(self):
        self.master.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.master.datadir = "/data"
        self.primary.datadir = "/data"

        gpArray = GpArray([self.master, self.primary])

        with self.assertRaisesRegexp(Exception, r"Segment dbid's 1 and 2 on host samehost cannot have the same data directory '/data'"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

if __name__ == '__main__':
    run_tests()
