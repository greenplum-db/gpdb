#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import unittest
from mock import call, Mock, patch, MagicMock
from gppylib.commands import pg
from test.unit.gp_unittest import GpTestCase, run_tests
from psycopg2 import DatabaseError

from gppylib.test.unit.gp_unittest import GpTestCase
from gppylib.commands.base import CommandResult

class TestUnitPgReplicationSlot(GpTestCase):
    def setUp(self):
        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
        self.replication_slot_name = "internal_wal_replication_slot"
        self.source_host = "bar"
        self.source_port = 1234

        self.pg_replication_slot = pg.PgReplicationSlot(
            self.source_host,
            self.source_port,
            self.replication_slot_name,
        )
        self.apply_patches([
            patch('gppylib.commands.pg.logger', return_value=mock_logger),
            patch('gppylib.db.dbconn.DbURL', return_value=Mock())
        ])

        self.mock_logger = self.get_mock_from_apply_patch('logger')

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_slot_exist_conn_exception(self, mock1):

        with self.assertRaises(Exception) as ex:
            self.pg_replication_slot.slot_exists()

        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Checking if slot internal_wal_replication_slot exists for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)
        self.assertTrue('Failed to query pg_replication_slots for' in str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.querySingleton', return_value=1)
    def test_slot_exist_query_true(self, mock1, mock2):
        self.assertTrue(self.pg_replication_slot.slot_exists())
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Checking if slot internal_wal_replication_slot exists for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.querySingleton', return_value=0)
    def test_slot_exist_query_false(self, mock1, mock2):
        self.assertFalse(self.pg_replication_slot.slot_exists())
        self.assertEqual(2, self.mock_logger.debug.call_count)
        self.assertEqual([call('Checking if slot internal_wal_replication_slot exists for host:bar, port:1234'),
                          call('Slot internal_wal_replication_slot does not exist for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_drop_slot_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            self.pg_replication_slot.drop_slot()

        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Dropping slot internal_wal_replication_slot for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)
        self.assertTrue('Failed to drop replication slot for host:bar, port:1234' in str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', side_effect=DatabaseError("DatabaseError Exception"))
    def test_drop_slot_db_error_exception(self, mock1, mock2):
        self.pg_replication_slot.drop_slot()
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual(1, self.mock_logger.exception.call_count)
        self.assertEqual([call('Dropping slot internal_wal_replication_slot for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual([call('Failed to query pg_drop_replication_slot for host:bar, port:1234: DatabaseError Exception')],
                         self.mock_logger.exception.call_args_list)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', autospec=True)
    def test_drop_slot_success(self, mock1, mock2):
        self.assertTrue(self.pg_replication_slot.drop_slot())
        self.assertEqual(2, self.mock_logger.debug.call_count)
        self.assertEqual([call('Dropping slot internal_wal_replication_slot for host:bar, port:1234'),
                          call('Successfully dropped replication slot internal_wal_replication_slot for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_create_slot_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            self.pg_replication_slot.create_slot()

        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Creating slot internal_wal_replication_slot for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)
        self.assertTrue('Failed to create replication slot for host:bar, port:1234' in str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', side_effect=DatabaseError("DatabaseError Exception"))
    def test_create_slot_db_error_exception(self, mock1, mock2):
        self.pg_replication_slot.create_slot()
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual(1, self.mock_logger.exception.call_count)
        self.assertEqual([call('Creating slot internal_wal_replication_slot for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual(
            [call('Failed to query pg_create_physical_replication_slot for host:bar, port:1234: DatabaseError Exception')],
            self.mock_logger.exception.call_args_list)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', autospec=True)
    def test_create_slot_success(self, mock1, mock2):
        self.assertTrue(self.pg_replication_slot.create_slot())
        self.assertEqual(2, self.mock_logger.debug.call_count)
        self.assertEqual([call('Creating slot internal_wal_replication_slot for host:bar, port:1234'),
                          call(
                              'Successfully created replication slot internal_wal_replication_slot for host:bar, port:1234')],
                         self.mock_logger.debug.call_args_list)

class TestUnitPgBaseBackup(unittest.TestCase):
    def test_replication_slot_not_passed_when_not_given_slot_name(self):
        base_backup = pg.PgBaseBackup(
            replication_slot_name = None,
            target_datadir="foo",
            source_host = "bar",
            source_port="baz",
            )

        tokens = base_backup.command_tokens

        self.assertNotIn("--slot", tokens)
        self.assertNotIn("some-replication-slot-name", tokens)
        self.assertIn("--wal-method", tokens)
        self.assertIn("fetch", tokens)
        self.assertNotIn("stream", tokens)

    @patch('gppylib.commands.pg.PgReplicationSlot.slot_exists', return_value=True)
    @patch('gppylib.commands.pg.PgReplicationSlot.drop_slot', return_value=True)
    def test_base_backup_passes_parameters_necessary_to_create_replication_slot_when_given_slotname(self, mock1, mock2):
        base_backup = pg.PgBaseBackup(
            create_slot=True,
            replication_slot_name='some-replication-slot-name',
            target_datadir="foo",
            source_host="bar",
            source_port="baz",
            )

        self.assertIn("--slot", base_backup.command_tokens)
        self.assertIn("some-replication-slot-name", base_backup.command_tokens)
        self.assertIn("--wal-method", base_backup.command_tokens)
        self.assertIn("stream", base_backup.command_tokens)
        self.assertIn("--create-slot", base_backup.command_tokens)

    @patch('gppylib.commands.pg.PgReplicationSlot.slot_exists', return_value=True)
    @patch('gppylib.commands.pg.PgReplicationSlot.drop_slot', return_value=False)
    def test_base_backup_passes_parameters_necessary_when_drop_replication_slot_ret_false(self, mock1, mock2):
        base_backup = pg.PgBaseBackup(
            create_slot=True,
            replication_slot_name='some-replication-slot-name',
            target_datadir="foo",
            source_host="bar",
            source_port="baz",
            )

        self.assertIn("--slot", base_backup.command_tokens)
        self.assertIn("some-replication-slot-name", base_backup.command_tokens)
        self.assertIn("--wal-method", base_backup.command_tokens)
        self.assertIn("stream", base_backup.command_tokens)
        self.assertNotIn("--create-slot", base_backup.command_tokens)

    @patch('gppylib.commands.pg.PgReplicationSlot.slot_exists', return_value=True)
    @patch('gppylib.commands.pg.PgReplicationSlot.drop_slot', return_value=True)
    def test_base_backup_does_not_pass_conflicting_xlog_method_argument_when_given_replication_slot(self, mock1, mock2):
        base_backup = pg.PgBaseBackup(
            create_slot=True,
            replication_slot_name='some-replication-slot-name',
            target_datadir="foo",
            source_host="bar",
            source_port="baz",
            )
        self.assertNotIn("-x", base_backup.command_tokens)
        self.assertNotIn("--xlog", base_backup.command_tokens)

class PgTests(GpTestCase):
    def setUp(self):
        self.apply_patches([
            patch('gppylib.commands.pg.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error', 'warning'])),
            patch('gppylib.db.dbconn.connect'),
            patch('gppylib.db.dbconn.querySingleton')
        ])
        self.logger = self.get_mock_from_apply_patch('logger')
        self.connect = self.get_mock_from_apply_patch('connect')
        self.query = self.get_mock_from_apply_patch('querySingleton')

    def test_kill_existing_walsenders(self):
        primary_config = [("sdw1", 20000), ("sdw1", 20001)]

        # Prepare mock return values
        self.query.return_value = True

        # Run the function being tested
        pg.kill_existing_walsenders_on_primary(primary_config)

        # Assertions
        expected_calls = [
            call.info('killing existing walsender process on primary sdw1:20000 to refresh replication connection'),
            call.info('killing existing walsender process on primary sdw1:20001 to refresh replication connection')
        ]
        self.logger.info.assert_has_calls(expected_calls)
        self.logger.warning.assert_not_called()

    def test_kill_existing_walsenders_failure(self):
        primary_config = [("sdw1", 20000), ("sdw1", 20001)]

        # Prepare mock return values
        self.query.return_value = False

        # Run the function being tested
        pg.kill_existing_walsenders_on_primary(primary_config)

        # Assertions
        expected_calls = [
            call.info('killing existing walsender process on primary sdw1:20000 to refresh replication connection'),
            call.info('killing existing walsender process on primary sdw1:20001 to refresh replication connection')
        ]
        self.logger.info.assert_has_calls(expected_calls)
        assert self.logger.warning.call_count == 2
        args, _ = self.logger.warning.call_args
        assert "Unable to kill walsender on primary host" in args[0]

    def test_kill_existing_walsenders_one_host_failure_and_other_succeed(self):
        primary_config = [("sdw1", 20000), ("sdw2", 20001)]

        # Prepare mock return values
        self.query.side_effect = [False, True]

        # Run the function being tested
        pg.kill_existing_walsenders_on_primary(primary_config)

        # Assertions
        self.logger.warning.assert_called_once()
        self.assertEqual([call("Unable to kill walsender on primary host {0}:{1}", "sdw1", 20000)],
                         self.logger.warning.call_args_list)

if __name__ == '__main__':
    run_tests()
