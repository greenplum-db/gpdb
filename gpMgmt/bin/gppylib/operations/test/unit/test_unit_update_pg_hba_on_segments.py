#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
from mock import call, patch, MagicMock, Mock
import os

from gppylib.commands.base import REMOTE
from gppylib.gparray import Segment, GpArray
from gppylib.operations.update_pg_hba_on_segments import update_pg_hba_on_segments, create_entries
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests


class UpdatePgHBAConfTests(GpTestCase):
    def setUp(self):
        def _setup_gparray():
            coordinator = Segment.initFromString(
                "1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
            standby = Segment.initFromString(
                "6|-1|m|m|s|u|sdw3|sdw3|5433|/data/standby")
            primary0 = Segment.initFromString(
                "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
            primary1 = Segment.initFromString(
                "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
            mirror0 = Segment.initFromString(
                "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
            mirror1 = Segment.initFromString(
                "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
            return GpArray([coordinator, standby, primary0, primary1, mirror0, mirror1])

        self.gparray = _setup_gparray()
        self.apply_patches([
            patch('gppylib.operations.update_pg_hba_on_segments.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error', 'warning'])),
            patch('gppylib.commands.unix.getUserName', return_value="gpadmin")
        ])
        self.logger = self.get_mock_from_apply_patch('logger')

        self.entries_block = """
host replication gpadmin samehost trust
host all gpadmin {ip_mirror1}/32 trust
host all gpadmin {ip_mirror2}/32 trust
host replication gpadmin {ip_mirror1}/32 trust
host replication gpadmin {ip_mirror2}/32 trust
host replication gpadmin {ip_primary1}/32 trust
host replication gpadmin {ip_primary2}/32 trust"""

    @patch('gppylib.operations.update_pg_hba_on_segments.gp.IfAddrs.list_addrs', return_value=['192.168.1.1', '192.168.2.1'])
    def test_create_entries(self, mock_ifaddrs):
        pair0 = self.gparray.getSegmentList()[0]
        primary_hostname = pair0.primaryDB.getSegmentHostName()
        mirror_hostname = pair0.mirrorDB.getSegmentHostName()
        entries = create_entries(primary_hostname, mirror_hostname, False)
        entries_str = "\n".join(entries)
        entries_expected = self.entries_block.format(ip_primary1 = '192.168.1.1', ip_primary2 = '192.168.2.1', ip_mirror1 = '192.168.1.1', ip_mirror2 = '192.168.2.1')
        self.assertEqual(entries_str, entries_expected)

    @patch('socket.gethostbyaddr', return_value=['sdw1', '', ''])
    def test_create_entries_hba_hostnames_true(self, gethost):
        pair0 = self.gparray.getSegmentList()[0]
        primary_hostname = pair0.primaryDB.getSegmentHostName()
        mirror_hostname = pair0.mirrorDB.getSegmentHostName()
        entries = create_entries(primary_hostname, mirror_hostname, True)
        entries_str = "\n".join(entries)
        entries_expected = "\nhost replication gpadmin samehost trust\nhost all gpadmin sdw1 trust\nhost replication gpadmin sdw1 trust"
        self.assertEqual(entries_str, entries_expected)

    @patch('gppylib.operations.update_pg_hba_on_segments.gp.IfAddrs.list_addrs', return_value=[])
    def test_create_entries_no_ifaddrs(self, mock_ifaddrs):
        pair0 = self.gparray.getSegmentList()[0]
        primary_hostname = pair0.primaryDB.getSegmentHostName()
        mirror_hostname = pair0.mirrorDB.getSegmentHostName()
        entries = create_entries(primary_hostname, mirror_hostname, False)
        entries_str = "\n".join(entries)
        entries_expected = "host replication gpadmin samehost trust"
        self.assertEqual(entries_str.strip(), entries_expected.strip())

    @patch('gppylib.operations.update_pg_hba_on_segments.update_on_segments')
    @patch('gppylib.operations.update_pg_hba_on_segments.create_entries', 
            side_effect=[['entry0', 'entry1'], ['entry1', 'entry2']])
    def test_update_pg_hba_on_segments_updated_successfully_all_failed_segments(self, mock_entry, mock_update):
        os.environ["GPHOME"] = "/usr/local/gpdb"
        expected_batch_size = 16
        update_pg_hba_on_segments(self.gparray, False, expected_batch_size)
        self.logger.info.assert_any_call("Starting to create new pg_hba.conf on primary segments")
        self.logger.info.assert_any_call("Successfully modified pg_hba.conf on primary segments to allow replication connections")

        self.assertEqual(mock_update.call_count, 1)
        mock_call_args = mock_update.call_args[0]

        result_cmds = mock_call_args[0]
        result_batch_size = mock_call_args[1]

        expected_string0 = "$GPHOME/sbin/seg_update_pg_hba.py --data-dir /data/primary0 --entries 'entry0\nentry1'"
        expected_string1 = "$GPHOME/sbin/seg_update_pg_hba.py --data-dir /data/primary1 --entries 'entry1\nentry2'"

        self.assertEqual(len(result_cmds), 2)
        self.assertEqual(result_batch_size, expected_batch_size)
        self.assertEqual(result_cmds[0].cmdStr, expected_string0)
        self.assertEqual(result_cmds[1].cmdStr, expected_string1)

    @patch('gppylib.operations.update_pg_hba_on_segments.update_on_segments')
    @patch('gppylib.operations.update_pg_hba_on_segments.create_entries', side_effect=[['entry0', 'entry1']])
    def test_one_primary_seg_unreachable(self, mock_entries, mock_update):
        pair0, pair1 = self.gparray.getSegmentList()
        pair0.primaryDB.unreachable = True

        os.environ["GPHOME"] = "/usr/local/gpdb"
        expected_batch_size = 16
        update_pg_hba_on_segments(self.gparray, False, expected_batch_size)

        self.assertEqual(mock_update.call_count, 1)
        mock_call_args = mock_update.call_args[0]

        result_cmds = mock_call_args[0]
        result_batch_size = mock_call_args[1]

        expected_string = "$GPHOME/sbin/seg_update_pg_hba.py --data-dir /data/primary1 --entries 'entry0\nentry1'"

        self.assertEqual(len(result_cmds), 1)
        self.assertEqual(result_batch_size, expected_batch_size)
        self.assertEqual(result_cmds[0].cmdStr, expected_string)

    @patch('gppylib.operations.update_pg_hba_on_segments.update_on_segments')
    @patch('gppylib.operations.update_pg_hba_on_segments.create_entries', side_effect=[['entry0', 'entry1']])
    def test_mirror_seg_is_none(self, mock_entries, mock_update):
        pair0, pair1 = self.gparray.getSegmentList()
        pair0.mirrorDB = None
        os.environ["GPHOME"] = "/usr/local/gpdb"
        expected_batch_size = 16
        update_pg_hba_on_segments(self.gparray, False, expected_batch_size)

        self.assertEqual(mock_update.call_count, 1)
        mock_call_args = mock_update.call_args[0]

        result_cmds = mock_call_args[0]
        result_batch_size = mock_call_args[1]

        expected_string = "$GPHOME/sbin/seg_update_pg_hba.py --data-dir /data/primary1 --entries 'entry0\nentry1'"

        self.assertEqual(len(result_cmds), 1)
        self.assertEqual(result_batch_size, expected_batch_size)
        self.assertEqual(result_cmds[0].cmdStr, expected_string)

    @patch('gppylib.operations.update_pg_hba_on_segments.update_on_segments')
    @patch('gppylib.operations.update_pg_hba_on_segments.create_entries')
    def test_both_seg_unreachable(self, mock_entries, mock_update):
        pair0, pair1 = self.gparray.getSegmentList()
        pair0.primaryDB.unreachable = True
        pair1.primaryDB.unreachable = True
        os.environ["GPHOME"] = "/usr/local/gpdb"
        expected_batch_size = 16
        update_pg_hba_on_segments(self.gparray, False, expected_batch_size)

        self.logger.warning.assert_any_call("Not updating pg_hba.conf for segments on unreachable hosts: sdw1, sdw2."
                                               "You can manually update pg_hba.conf once you make the hosts reachable.")
        self.logger.info.assert_any_call("None of the reachable segments require update to pg_hba.conf")
        self.assertEqual(mock_update.call_count, 0)

    @patch('gppylib.operations.update_pg_hba_on_segments.update_on_segments')
    @patch('gppylib.operations.update_pg_hba_on_segments.create_entries', side_effect=[['entry0', 'entry1']])
    def test_one_seg_to_update(self, mock_entries, mock_update):
        pair0, pair1 = self.gparray.getSegmentList()
        os.environ["GPHOME"] = "/usr/local/gpdb"
        expected_batch_size = 16
        update_pg_hba_on_segments(self.gparray, False, expected_batch_size, contents_to_update=[0])

        self.assertEqual(mock_update.call_count, 1)
        mock_call_args = mock_update.call_args[0]

        result_cmds = mock_call_args[0]
        result_batch_size = mock_call_args[1]

        expected_string = "$GPHOME/sbin/seg_update_pg_hba.py --data-dir /data/primary0 --entries 'entry0\nentry1'"

        self.assertEqual(len(result_cmds), 1)
        self.assertEqual(result_batch_size, expected_batch_size)
        self.assertEqual(result_cmds[0].cmdStr, expected_string)
