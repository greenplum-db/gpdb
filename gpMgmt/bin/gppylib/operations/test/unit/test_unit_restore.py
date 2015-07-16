#!/usr/bin/env python
# coding: utf-8
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#

import sys
import unittest2 as unittest
import tempfile, os, shutil
from gppylib.commands.base import CommandResult
from gppylib.operations.restore import RestoreDatabase, create_restore_plan, get_plan_file_contents, \
        get_restore_tables_from_table_file, restore_incremental_data_only, write_to_plan_file, \
        validate_tablenames, create_plan_file_contents, GetDbName, get_dirty_table_file_contents, \
        get_incremental_restore_timestamps, get_partition_list, get_restore_dir, is_begin_incremental_run, \
        is_incremental_restore, get_restore_table_list, validate_restore_tables_list, \
        update_ao_stat_func, update_ao_statistics, _build_gpdbrestore_cmd_line, ValidateTimestamp, \
        is_full_restore, restore_state_files_with_nbu, restore_report_file_with_nbu, restore_cdatabase_file_with_nbu, \
        restore_global_file_with_nbu, restore_config_files_with_nbu, config_files_dumped, global_file_dumped, \
        restore_partition_list_file_with_nbu, restore_increments_file_with_nbu

from gppylib.commands.base import ExecutionError
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from mock import patch, MagicMock, Mock

class restoreTestCase(unittest.TestCase):

    def setUp(self):
        self.restore = RestoreDatabase(restore_timestamp = '20121212121212',
                                       no_analyze = True,
                                       drop_db = True,
                                       restore_global = False,
                                       master_datadir = 'foo',
                                       backup_dir = None,
                                       master_port = 0,
                                       dump_dir = "db_dumps",
                                       dump_prefix = "",
                                       no_plan = False,
                                       restore_tables = None,
                                       batch_default=64,
                                       no_ao_stats = False,
                                       redirected_restore_db = None,
                                       report_status_dir = None,
                                       ddboost = False,
                                       netbackup_service_host = None,
                                       netbackup_block_size = None)

    def create_backup_dirs(self, top_dir=os.getcwd(), dump_dirs=[]):
        if dump_dirs is None:
            return

        for dump_dir in dump_dirs:
            backup_dir = os.path.join(top_dir, 'db_dumps', dump_dir)
            if not os.path.isdir(backup_dir):
                os.makedirs(backup_dir)
                if not os.path.isdir(backup_dir):
                    raise Exception('Failed to create directory %s' % backup_dir)

    def remove_backup_dirs(self, top_dir=os.getcwd(), dump_dirs=[]):
        if dump_dirs is None:
            return

        for dump_dir in dump_dirs:
            backup_dir = os.path.join(top_dir, 'db_dumps', dump_dir)
            shutil.rmtree(backup_dir)
            if os.path.isdir(backup_dir):
                raise Exception('Failed to remove directory %s' % backup_dir)

    def test_GetDbName_1(self):
        """ Basic test """
        with tempfile.NamedTemporaryFile() as f:
            f.write("""
--
-- Database creation
--

CREATE DATABASE monkey WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = thisguy;
""")
            f.flush()
            self.assertTrue(GetDbName(f.name).run() == "monkey")

    def test_GetDbName_2(self):
        """ Verify that GetDbName looks no further than 50 lines. """
        with tempfile.NamedTemporaryFile() as f:
            for i in range(0, 50):
                f.write("crap\n")
            f.write("CREATE DATABASE monkey")
            f.flush()
            try:
                GetDbName(f.name).run()
            except GetDbName.DbNameGiveUp, e:
                return
            self.fail("DbNameGiveUp should have been raised.")

    def test_GetDbName_3(self):
        """ Verify that GetDbName fails  when cdatabase file ends prematurely. """
        with tempfile.NamedTemporaryFile() as f:
            f.write("this is the whole file")
            f.flush()
            try:
                GetDbName(f.name).run()
            except GetDbName.DbNameNotFound, e:
                return
            self.fail("DbNameNotFound should have been raised.")

    @patch('gppylib.operations.restore.RestoreDatabase._process_createdb', side_effect=ExceptionNoStackTraceNeeded('Failed to create database'))
    @patch('time.sleep')
    def test_multitry_createdb_1(self, mock1, mock2):
        r = RestoreDatabase('20121219', True, True, False, 'FOO', None, 1234, False, False, None, None, 'db_dumps', '', False, None, None, None, None)
        self.assertRaises(ExceptionNoStackTraceNeeded, r._multitry_createdb, '20121219', 'fullbkdb', None, 'FOO', None, 1234)

    @patch('gppylib.operations.restore.RestoreDatabase._process_createdb')
    def test_multitry_createdb_2(self, mock):
        r = RestoreDatabase('20121219', True, True, False, 'FOO', None, 1234, False, False, None, None, 'db_dumps', '', False, None, None, None, None)
        r._multitry_createdb('20121219', 'fullbkdb', None, 'FOO', None, 1234)

    @patch('gppylib.operations.restore.get_partition_list', return_value=[('public', 't1'), ('public', 't2'), ('public', 't3')])
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental', return_value='123456789')
    @patch('gppylib.operations.restore.get_incremental_restore_timestamps', return_value=['20121212121212', '20121212121211'])
    @patch('gppylib.operations.restore.get_dirty_table_file_contents', return_value=['public.t1', 'public.t2'])
    def test_restore_plan_file_00(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        db_timestamp = '01234567891234'
        dbname = 'bkdb'
        ddboost = False
        backup_dir = None
        netbackup_service_host = None
        netbackup_block_size = None
        self.create_backup_dirs(master_datadir, [db_timestamp[0:8]])
        plan_file = create_restore_plan(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, db_timestamp, ddboost, netbackup_service_host, netbackup_block_size)
        self.assertTrue(os.path.isfile(plan_file))
        self.remove_backup_dirs(master_datadir, [db_timestamp[0:8]])

    @patch('gppylib.operations.restore.get_partition_list', return_value=[])
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental', return_value='123456789')
    @patch('gppylib.operations.restore.get_incremental_restore_timestamps', return_value=['20121212121212', '20121212121211'])
    @patch('gppylib.operations.restore.get_dirty_table_file_contents', return_value=['public.t1', 'public.t2'])
    def test_restore_plan_file_01(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        db_timestamp = '01234567891234'
        dbname = 'bkdb'
        ddboost = False
        backup_dir = None
        netbackup_service_host = None
        netbackup_block_size = None
        self.create_backup_dirs(master_datadir, [db_timestamp[0:8]])
        plan_file = create_restore_plan(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, db_timestamp, ddboost, netbackup_service_host, netbackup_block_size)
        self.assertTrue(os.path.isfile(plan_file))
        self.remove_backup_dirs(master_datadir, [db_timestamp[0:8]])


    @patch('gppylib.operations.restore.get_partition_list', return_value=[])
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental', return_value=None)
    def test_restore_plan_file_02(self, mock1, mock2):
        master_datadir = 'foo'
        db_timestamp = '01234567891234'
        dbname = 'bkdb'
        ddboost = False
        backup_dir = None
        netbackup_service_host = None
        netbackup_block_size = None
        self.create_backup_dirs(master_datadir, [db_timestamp[0:8]])
        with self.assertRaisesRegexp(Exception, 'Could not locate fullbackup associated with ts'):
            create_restore_plan(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, db_timestamp, ddboost, netbackup_service_host, netbackup_block_size)
        self.remove_backup_dirs(master_datadir, [db_timestamp[0:8]])

    @patch('gppylib.operations.restore.get_partition_list', return_value=[])
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20120101000000')
    @patch('gppylib.operations.restore.get_incremental_restore_timestamps', return_value=['20121212121212', '20121212121211'])
    @patch('gppylib.operations.restore.get_dirty_table_file_contents', return_value=['public.t1', 'public.t2'])
    @patch('gppylib.operations.restore.create_plan_file_contents')
    def test_restore_plan_file_03(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = 'foo'
        db_timestamp = '20140101000000'
        dbname = 'bkdb'
        ddboost = False
        backup_dir = None
        netbackup_service_host = 'mdw'
        netbackup_block_size = '1024'
        self.create_backup_dirs(master_datadir, [db_timestamp[0:8]])
        plan_file = create_restore_plan(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, db_timestamp, ddboost, netbackup_service_host, netbackup_block_size)
        self.assertTrue(os.path.isfile(plan_file))
        self.remove_backup_dirs(master_datadir, [db_timestamp[0:8]])

    @patch('gppylib.operations.restore.get_partition_list', return_value=[])
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value=None)
    def test_restore_plan_file_04(self, mock1, mock2):
        master_datadir = 'foo'
        db_timestamp = '01234567891234'
        dbname = 'bkdb'
        ddboost = False
        backup_dir = None
        netbackup_service_host = 'mdw'
        netbackup_block_size = '1024'
        self.create_backup_dirs(master_datadir, [db_timestamp[0:8]])
        with self.assertRaisesRegexp(Exception, 'Could not locate fullbackup associated with ts'):
            create_restore_plan(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, db_timestamp, ddboost, netbackup_service_host, netbackup_block_size)
        self.remove_backup_dirs(master_datadir, [db_timestamp[0:8]])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20121212121210', '20121212121209', '20121212121208', '20121212121207', '20121212121206', '20121212121205', '20121212121204', '20121212121203', '20121212121202', '20121212121201'])
    def test_get_incremental_restore_timestamps_00(self, mock):
        master_data_dir = 'foo'
        latest_full_timestamp = '20121212121201'
        restore_timestamp = '20121212121205'
        backup_dir = None
        increments = get_incremental_restore_timestamps(master_data_dir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, latest_full_timestamp, restore_timestamp)
        self.assertEqual(increments, ['20121212121205', '20121212121204', '20121212121203', '20121212121202', '20121212121201'])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20121212121210', '20121212121209', '20121212121208', '20121212121207', '20121212121206', '20121212121205', '20121212121204', '20121212121203', '20121212121202', '20121212121201'])
    def test_get_incremental_restore_timestamps_01(self, mock):
        master_data_dir = 'foo'
        latest_full_timestamp = '20121212121201'
        restore_timestamp = '20121212121210'
        backup_dir = None
        increments = get_incremental_restore_timestamps(master_data_dir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, latest_full_timestamp, restore_timestamp)
        self.assertEqual(increments, ['20121212121210', '20121212121209', '20121212121208', '20121212121207', '20121212121206', '20121212121205', '20121212121204', '20121212121203', '20121212121202', '20121212121201'])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=[])
    def test_get_incremental_restore_timestamps_03(self, mock):
        master_data_dir = 'foo'
        latest_full_timestamp = '20121212121201'
        restore_timestamp = '20121212121200'
        backup_dir = None
        increments = get_incremental_restore_timestamps(master_data_dir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, latest_full_timestamp, restore_timestamp)
        self.assertEqual(increments, [])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['public.t1', 'public.t2', 'public.t3'])
    def test_get_dirty_table_file_contents_00(self, mock):
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '20121212121212'
        dirty_tables = get_dirty_table_file_contents(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp_key)
        self.assertEqual(dirty_tables, ['public.t1', 'public.t2', 'public.t3'])

    @patch('gppylib.operations.restore.get_lines_from_file', side_effect=[['public.t1'], ['public.t1', 'public.t2', 'public.t3'], ['public.t2', 'public.t4']])
    def test_create_plan_file_contents_00(self, mock):
        master_datadir = 'foo'
        table_set_from_metadata_file = ['public.t1', 'public.t2', 'public.t3', 'public.t4']
        incremental_restore_timestamps = ['20121212121213', '20121212121212', '20121212121211']
        latest_full_timestamp = '20121212121210'
        backup_dir = None
        netbackup_service_host = None
        netbackup_block_size = None
        expected_output = {'20121212121213': ['public.t1'], '20121212121212': ['public.t2', 'public.t3'], '20121212121211': ['public.t4'], '20121212121210': []}
        file_contents = create_plan_file_contents(master_datadir,
                                                  backup_dir,
                                                  self.restore.dump_dir,
                                                  self.restore.dump_prefix,
                                                  table_set_from_metadata_file,
                                                  incremental_restore_timestamps,
                                                  latest_full_timestamp,
                                                  netbackup_service_host,
                                                  netbackup_block_size)
        self.assertEqual(file_contents, expected_output)

    def test_create_plan_file_contents_01(self):
        master_datadir = 'foo'
        table_set_from_metadata_file = ['public.t1', 'public.t2', 'public.t3', 'public.t4']
        incremental_restore_timestamps = []
        latest_full_timestamp = '20121212121210'
        backup_dir = None
        netbackup_service_host = None
        netbackup_block_size = None
        expected_output = {'20121212121210': ['public.t1', 'public.t2', 'public.t3', 'public.t4']}
        file_contents = create_plan_file_contents(master_datadir,
                                                  backup_dir,
                                                  self.restore.dump_dir,
                                                  self.restore.dump_prefix,
                                                  table_set_from_metadata_file,
                                                  incremental_restore_timestamps,
                                                  latest_full_timestamp,
                                                  netbackup_service_host,
                                                  netbackup_block_size)
        self.assertEqual(file_contents, expected_output)

    @patch('gppylib.operations.restore.get_lines_from_file', side_effect=[['public.t1'], ['public.t1', 'public.t2', 'public.t3'], ['public.t2', 'public.t4']])
    def test_create_plan_file_contents_02(self, mock):
        master_datadir = 'foo'
        table_set_from_metadata_file = []
        incremental_restore_timestamps = ['20121212121213', '20121212121212', '20121212121211']
        latest_full_timestamp = '20121212121210'
        backup_dir = None
        netbackup_service_host = None
        netbackup_block_size = None
        expected_output = {'20121212121212': [], '20121212121213': [], '20121212121211': [], '20121212121210': []}
        file_contents = create_plan_file_contents(master_datadir,
                                                  backup_dir,
                                                  self.restore.dump_dir,
                                                  self.restore.dump_prefix,
                                                  table_set_from_metadata_file,
                                                  incremental_restore_timestamps,
                                                  latest_full_timestamp,
                                                  netbackup_service_host,
                                                  netbackup_block_size)
        self.assertEqual(file_contents, expected_output)

    @patch('gppylib.operations.restore.get_lines_from_file', side_effect=[['public.t1'], ['public.t1', 'public.t2', 'public.t3'], ['public.t2', 'public.t4']])
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_create_plan_file_contents_03(self, mock1, mock2):
        master_datadir = 'foo'
        table_set_from_metadata_file = []
        incremental_restore_timestamps = ['20121212121213', '20121212121212', '20121212121211']
        latest_full_timestamp = '20121212121210'
        backup_dir = None
        netbackup_service_host = 'mdw'
        netbackup_block_size = '1024'
        expected_output = {'20121212121212': [], '20121212121213': [], '20121212121211': [], '20121212121210': []}
        file_contents = create_plan_file_contents(master_datadir,
                                                  backup_dir,
                                                  self.restore.dump_dir,
                                                  self.restore.dump_prefix,
                                                  table_set_from_metadata_file,
                                                  incremental_restore_timestamps,
                                                  latest_full_timestamp,
                                                  netbackup_service_host,
                                                  netbackup_block_size)
        self.assertEqual(file_contents, expected_output)

    @patch('gppylib.operations.restore.write_lines_to_file')
    @patch('gppylib.operations.restore.verify_lines_in_file')
    def test_write_to_plan_file_00(self, mock1, mock2):
        plan_file = 'blah'
        plan_file_contents = {'20121212121213': ['public.t1'],
                              '20121212121212': ['public.t2', 'public.t3'],
                              '20121212121211': ['public.t4']}

        expected_output = ['20121212121213:public.t1',
                           '20121212121212:public.t2,public.t3',
                           '20121212121211:public.t4']

        file_contents = write_to_plan_file(plan_file_contents, plan_file)
        self.assertEqual(expected_output, file_contents)

    @patch('gppylib.operations.restore.write_lines_to_file')
    @patch('gppylib.operations.restore.verify_lines_in_file')
    def test_write_to_plan_file_01(self, mock1, mock2):
        plan_file = 'blah'
        plan_file_contents = {}
        expected_output = []
        file_contents = write_to_plan_file(plan_file_contents, plan_file)
        self.assertEqual(expected_output, file_contents)

    @patch('gppylib.operations.restore.write_lines_to_file')
    @patch('gppylib.operations.restore.verify_lines_in_file')
    def test_write_to_plan_file_02(self, mock1, mock2):
        plan_file = None
        plan_file_contents = {}
        with self.assertRaisesRegexp(Exception, 'Invalid plan file .*'):
            write_to_plan_file(plan_file_contents, plan_file)

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['public.t1', 'public.t2'])
    def test_get_partition_list_00(self, mock):
        master_datadir = 'foo'
        backup_dir = None
        timestamp = '20121212121212'
        partition_list = get_partition_list(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp)
        self.assertEqual(partition_list, [('public', 't1'), ('public', 't2')])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=[])
    def test_get_partition_list_01(self, mock):
        master_datadir = 'foo'
        backup_dir = None
        timestamp = '20121212121212'
        partition_list = get_partition_list(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp)
        self.assertEqual(partition_list, [])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Incremental'])
    @patch('os.path.isfile', return_value=True)
    def test_is_incremental_restore_00(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertTrue(is_incremental_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=True)
    @patch('os.path.isfile', return_value=True)
    def test_is_incremental_restore_01(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = '/foo'
        self.assertTrue(is_incremental_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Full'])
    def test_is_incremental_restore_02(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertFalse(is_incremental_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=False)
    def test_is_incremental_restore_03(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertFalse(is_incremental_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.generate_report_filename', return_value='foo')
    @patch('os.path.isfile', return_value=False)
    def test_is_incremental_restore_04(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertFalse(is_incremental_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.generate_report_filename', return_value='foo')
    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Full'])
    @patch('os.path.isfile', return_value=True)
    def test_is_full_restore_00(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertTrue(is_full_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.generate_report_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=True)
    @patch('os.path.isfile', return_value=True)
    def test_is_full_restore_01(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = '/foo'
        self.assertTrue(is_full_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.generate_report_filename', return_value='foo')
    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Incremental'])
    def test_is_full_restore_02(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertFalse(is_full_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.generate_report_filename', return_value='foo')
    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=False)
    def test_is_full_restore_03(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        self.assertFalse(is_full_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp))

    @patch('gppylib.operations.restore.generate_report_filename', return_value='foo')
    @patch('os.path.isfile', return_value=False)
    def test_is_full_restore_04(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        with self.assertRaisesRegexp(Exception, 'Report file foo does not exist'):
            is_full_restore(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_00(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = False
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = False
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=db_dumps/20121212 -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_01(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        table_filter_file = None
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=db_dumps/20121212 --gp-c -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=True)
    def test_build_schema_only_restore_line_02(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-r=/foo/db_dumps/20121212 --status=/foo/db_dumps/20121212 --gp-d=/foo/db_dumps/20121212 --gp-c -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_schema_only_restore_line_03(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        self.restore.dump_prefix = 'bar_'
        table_filter_file = None
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=/foo/db_dumps/20121212 --prefix=bar_ --gp-c -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)


    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_schema_only_restore_line_04(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        self.restore.dump_prefix = 'bar_'
        table_filter_file = 'filter_file1'
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=/foo/db_dumps/20121212 --prefix=bar_ --gp-f=%s --gp-c -d bkdb' % (metadata_file, table_filter_file)

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_schema_only_restore_line_05(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=/foo/db_dumps/20121212 --gp-c -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_06(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = False
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-r=/tmp --status=/tmp --gp-d=/foo/db_dumps/20121212 --gp-c -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_07(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = True
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s -P --gp-r=/tmp --status=/tmp --gp-d=/foo/db_dumps/20121212 --gp-c -d bkdb' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_08(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = False
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = False
        self.restore.netbackup_service_host = "mdw"
        self.restore.netbackup_block_size = None
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=db_dumps/20121212 -d bkdb --netbackup-service-host=mdw' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_09(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = False
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = False
        self.restore.netbackup_service_host = "mdw"
        self.restore.netbackup_block_size = 1024
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s --gp-d=db_dumps/20121212 -d bkdb --netbackup-service-host=mdw --netbackup-block-size=1024' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_schema_only_restore_line_10(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = True
        self.restore.ddboost = True
        self.restore.dump_dir = '/backup/DCA-35'
        metadata_file = os.path.join(master_datadir, 'db_dumps', restore_timestamp[0:8], 'gp_dump_1_1_%s.gz' % restore_timestamp)
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p -s %s -P --gp-r=/tmp --status=/tmp --gp-d=/backup/DCA-35/20121212 --gp-c -d bkdb --ddboost' % metadata_file

        restore_line = self.restore._build_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, metadata_file, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_00(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = False
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p -P -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_01(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p --gp-c -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=True)
    def test_build_post_data_schema_only_restore_line_02(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p --gp-r=/foo/db_dumps/20121212 --status=/foo/db_dumps/20121212 --gp-c -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_post_data_schema_only_restore_line_03(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        self.restore.dump_prefix = 'bar_'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p --prefix=bar_ --gp-c -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)


    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_post_data_schema_only_restore_line_04(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        self.restore.dump_prefix = 'bar_'
        table_filter_file = 'filter_file1'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p --prefix=bar_ --gp-f=%s --gp-c -d bkdb' % (table_filter_file)

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_post_data_schema_only_restore_line_05(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p --gp-c -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_06(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_07(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p -P --gp-r=/tmp --status=/tmp --gp-c -d bkdb'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_08(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        table_filter_file = None
        full_restore_with_filter = True
        self.restore.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p -P --gp-r=/tmp --status=/tmp --gp-c -d bkdb --ddboost'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_09(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = True
        self.restore.netbackup_service_host = "mdw"
        self.restore.netbackup_block_size = None
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p -P --gp-c -d bkdb --netbackup-service-host=mdw'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_post_data_schema_only_restore_line_10(self, mock1, mock2):
        master_datadir = 'foo'
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        table_filter_file = None
        full_restore_with_filter = True
        self.restore.netbackup_service_host = "mdw"
        self.restore.netbackup_block_size = 1024
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20121212 --gp-i --gp-k=20121212121212 --gp-l=p -P --gp-c -d bkdb --netbackup-service-host=mdw --netbackup-block-size=1024'

        restore_line = self.restore._build_post_data_schema_only_restore_line(restore_timestamp, restore_db, compress, master_port, table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_00(self, mock1, mock2):
        ts = '20121212121212'
        dump_prefix = 'bar_'
        expected_output = 'gpdbrestore -t 20121212121212 --table-file foo -a -v --noplan --noanalyze --noaostats --prefix=bar'
        restore_line = _build_gpdbrestore_cmd_line(ts, 'foo', None, None, None, dump_prefix)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_redirected_restore_build_gpdbrestore_cmd_line_00(self, mock1, mock2):
        ts = '20121212121212'
        dump_prefix = 'bar_'
        expected_output = 'gpdbrestore -t 20121212121212 --table-file foo -a -v --noplan --noanalyze --noaostats --prefix=bar --redirect=redb'
        restore_line = _build_gpdbrestore_cmd_line(ts, 'foo', None, 'redb', None, dump_prefix)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_01(self, mock1, mock2):
        ts = '20121212121212'
        dump_prefix = 'bar_'
        expected_output = 'gpdbrestore -t 20121212121212 --table-file foo -a -v --noplan --noanalyze --noaostats -u /tmp --prefix=bar'
        restore_line = _build_gpdbrestore_cmd_line(ts, 'foo', '/tmp', None, None, dump_prefix)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_02(self, mock1, mock2):
        ts = '20121212121212'
        dump_prefix = 'bar_'
        report_status_dir = '/tmp'
        expected_output = 'gpdbrestore -t 20121212121212 --table-file foo -a -v --noplan --noanalyze --noaostats --prefix=bar --report-status-dir=/tmp'
        restore_line = _build_gpdbrestore_cmd_line(ts, 'foo', None, None, '/tmp', dump_prefix)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_03(self, mock1, mock2):
        ts = '20121212121212'
        dump_prefix = 'bar_'
        expected_output = 'gpdbrestore -t 20121212121212 --table-file foo -a -v --noplan --noanalyze --noaostats --prefix=bar --report-status-dir=/tmp --ddboost'
        ddboost = True
        restore_line = _build_gpdbrestore_cmd_line(ts, 'foo', None, None, '/tmp', dump_prefix, ddboost)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_redirected_restore_build_gpdbrestore_cmd_line_01(self, mock1, mock2):
        ts = '20121212121212'
        dump_prefix = 'bar_'
        expected_output = 'gpdbrestore -t 20121212121212 --table-file foo -a -v --noplan --noanalyze --noaostats -u /tmp --prefix=bar --redirect=redb'
        restore_line = _build_gpdbrestore_cmd_line(ts, 'foo', '/tmp', 'redb', None, dump_prefix)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_00(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = False
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-c -d bkdb -a'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_01(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = False
        no_ao_stats = False
        table_filter_file = '/tmp/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-f=/tmp/foo --gp-c -d bkdb'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_02(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = False
        table_filter_file = None
        full_restore_with_filter = False
        self.restore.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-c -d bkdb -a --ddboost'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_03(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.report_status_dir = '/tmp'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-r=/tmp --status=/tmp --gp-c -d bkdb -a --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_04(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-c -d bkdb -a --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_05(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = False
        no_ao_stats = True
        table_filter_file = '/tmp/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-f=/tmp/foo --gp-c -d bkdb --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_06(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        full_restore_with_filter = False
        self.restore.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-c -d bkdb -a --gp-nostats --ddboost'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_07(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.dump_prefix = 'bar_'
        full_restore_with_filter = False
        self.restore.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --prefix=bar_ --gp-k=20121212121212 --gp-l=p --gp-d=db_dumps/20121212 --gp-c -d bkdb -a --gp-nostats --ddboost'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=True)
    def test_build_restore_line_08(self, mock1, mock2, mock3):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.backup_dir = '/tmp'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=/tmp/db_dumps/20121212 --gp-r=/tmp/db_dumps/20121212 --status=/tmp/db_dumps/20121212 --gp-c -d bkdb -a --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.restore.RestoreDatabase.backup_dir_is_writable', return_value=False)
    def test_build_restore_line_09(self, mock1, mock2, mock3):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.backup_dir = '/tmp'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=/tmp/db_dumps/20121212 --gp-c -d bkdb -a --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_10(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=/foo/db_dumps/20121212 --gp-r=/tmp --status=/tmp --gp-c -d bkdb -a --gp-nostats'

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_11(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=/foo/db_dumps/20121212 --gp-r=/tmp --status=/tmp --gp-c -d bkdb -a --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_12(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = False
        no_ao_stats = True
        table_filter_file = None
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        full_restore_with_filter = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=/foo/db_dumps/20121212 --gp-r=/tmp --status=/tmp --gp-c -d bkdb -a --gp-nostats'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_13(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.report_status_dir = '/tmp'
        self.restore.backup_dir = '/foo'
        self.restore.netbackup_service_host = "mdw"
        full_restore_with_filter = False
        self.restore.ddboost = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=/foo/db_dumps/20121212 --gp-r=/tmp --status=/tmp --gp-c -d bkdb -a --gp-nostats --netbackup-service-host=mdw'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_restore_line_14(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        restore_db = 'bkdb'
        compress = True
        master_port = '5432'
        no_plan = True
        no_ao_stats = True
        table_filter_file = None
        self.restore.ddboost = True
        self.restore.report_status_dir = '/tmp'
        self.restore.netbackup_service_host = "mdw"
        full_restore_with_filter = False
        self.restore.dump_dir = 'backup/DCA-35'
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-i --gp-k=20121212121212 --gp-l=p --gp-d=backup/DCA-35/20121212 --gp-r=/tmp --status=/tmp --gp-c -d bkdb -a --gp-nostats --ddboost --netbackup-service-host=mdw'

        restore_line = self.restore._build_restore_line(restore_timestamp, restore_db, compress, master_port, no_plan, table_filter_file, no_ao_stats, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.generate_plan_filename', return_value='foo')
    def test_get_plan_file_contents_00(self, mock1):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        with self.assertRaisesRegexp(Exception, 'Plan file foo does not exist'):
            get_plan_file_contents(master_datadir, backup_dir, timestamp, self.restore.dump_dir, self.restore.dump_prefix)

    @patch('gppylib.operations.restore.generate_plan_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=[])
    @patch('os.path.isfile', return_value=True)
    def test_get_plan_file_contents_01(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        with self.assertRaisesRegexp(Exception, 'Plan file foo has no contents'):
            get_plan_file_contents(master_datadir, backup_dir, timestamp, self.restore.dump_dir, self.restore.dump_prefix)

    @patch('gppylib.operations.restore.generate_plan_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20121212121212:t1,t2', '20121212121211:t3,t4', '20121212121210:t5,t6,t7'])
    @patch('os.path.isfile', return_value=True)
    def test_get_plan_file_contents_02(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        expected_output = [('20121212121212','t1,t2'), ('20121212121211','t3,t4'), ('20121212121210','t5,t6,t7')]
        output = get_plan_file_contents(master_datadir, backup_dir, timestamp, self.restore.dump_dir, self.restore.dump_prefix)
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.restore.generate_plan_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20121212121212:', '20121212121211', '20121212121210:'])
    @patch('os.path.isfile', return_value=True)
    def test_get_plan_file_contents_03(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        with self.assertRaisesRegexp(Exception, 'Invalid plan file format'):
            get_plan_file_contents(master_datadir, backup_dir, timestamp, self.restore.dump_dir, self.restore.dump_prefix)

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20121212121212', 't1,t2'), ('20121212121211', 't3,t4'), ('20121212121210', 't5,t6,t7')])
    @patch('gppylib.operations.restore.Command.run')
    def test_restore_incremental_data_only_00(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        restore_tables = None
        netbackup_service_host = None
        netbackup_block_size = None
        results = restore_incremental_data_only(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, restore_tables, None, None, netbackup_service_host, netbackup_block_size)
        self.assertTrue(results)

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20121212121212', 't1,t2'), ('20121212121211', 't3,t4'), ('20121212121210', 't5,t6,t7')])
    @patch('gppylib.operations.restore.Command.run')
    def redirected_restore_test_restore_incremental_data_only_00(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        restore_tables = None
        netbackup_service_host = None
        netbackup_block_size = None
        results = restore_incremental_data_only(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, restore_tables, 'redb', None, netbackup_service_host, netbackup_block_size)
        self.assertTrue(results)

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20121212121212', ''), ('20121212121211', ''), ('20121212121210', '')])
    @patch('os.path.isfile', return_value=True)
    def test_restore_incremental_data_only_01(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        restore_tables = None
        netbackup_service_host = None
        netbackup_block_size = None
        with self.assertRaisesRegexp(Exception, 'There were no tables to restore. Check the plan file contents for restore timestamp %s' % timestamp):
            restore_incremental_data_only(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, restore_tables, 'redb', None, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20121212121212', 't1,t2'), ('20121212121211', 't3,t4'), ('20121212121210', 't5,t6,t7')])
    @patch('gppylib.operations.restore.Command.run')
    def test_restore_incremental_data_only_02(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        restore_tables = None
        netbackup_service_host = None
        netbackup_block_size = None
        self.assertTrue(restore_incremental_data_only(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, restore_tables, 'redb', None, netbackup_service_host, netbackup_block_size))

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20121212121212', 't1,t2'), ('20121212121211', 't3,t4'), ('20121212121210', 't5,t6,t7')])
    @patch('gppylib.operations.restore.Command.run', side_effect=Exception('Error executing gpdbrestore'))
    def test_restore_incremental_data_only_04(self, mock1, mock2):
        master_datadir = 'foo'
        timestamp = '20121212121212'
        backup_dir = None
        restore_tables = None
        netbackup_service_host = None
        netbackup_block_size = None
        with self.assertRaisesRegexp(Exception, 'Error executing gpdbrestore'):
            restore_incremental_data_only(master_datadir, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, restore_tables, 'redb', None, netbackup_service_host, netbackup_block_size)

    def test_get_restore_dir_00(self):
        master_datadir = '/foo'
        backup_dir = None
        self.assertEqual(get_restore_dir(master_datadir, backup_dir), '/foo')

    def test_get_restore_dir_01(self):
        master_datadir = None
        backup_dir = '/foo'
        self.assertEqual(get_restore_dir(master_datadir, backup_dir), '/foo')

    def test_get_restore_dir_02(self):
        master_datadir = None
        backup_dir = None
        self.assertEqual(get_restore_dir(master_datadir, backup_dir), None)

    @patch('gppylib.operations.restore.is_incremental_restore', return_value=True)
    def test_is_begin_incremental_run_00(self, m):
        mdd = '/foo'
        backup_dir = '/tmp'
        timestamp = '20130204135500'
        noplan = True
        result = is_begin_incremental_run(mdd, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, noplan)
        self.assertFalse(result)

    @patch('gppylib.operations.restore.is_incremental_restore', return_value=True)
    def test_is_begin_incremental_run_01(self, m):
        mdd = '/foo'
        backup_dir = '/tmp'
        timestamp = '20130204135500'
        noplan = False
        result = is_begin_incremental_run(mdd, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, noplan)
        self.assertTrue(result)

    @patch('gppylib.operations.restore.is_incremental_restore', return_value=False)
    def test_is_begin_incremental_run_02(self, m):
        mdd = '/foo'
        backup_dir = '/tmp'
        timestamp = '20130204135500'
        noplan = True
        result = is_begin_incremental_run(mdd, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, noplan)
        self.assertFalse(result)

    @patch('gppylib.operations.restore.is_incremental_restore', return_value=False)
    def test_is_begin_incremental_run_03(self, m):
        mdd = '/foo'
        backup_dir = '/tmp'
        timestamp = '20130204135500'
        noplan = False
        result = is_begin_incremental_run(mdd, backup_dir, self.restore.dump_dir, self.restore.dump_prefix, timestamp, noplan)
        self.assertFalse(result)

    def test_create_filter_file_00(self):
        self.restore.restore_tables = None
        fname = self.restore.create_filter_file()
        self.assertEquals(fname, None)

    @patch('gppylib.operations.restore.get_all_segment_addresses', return_value=['host1'])
    @patch('gppylib.operations.restore.scp_file_to_hosts')
    def test_create_filter_file_01(self, m1, m2):
        self.restore.restore_tables = ['public.ao1', 'pepper.heap1']
        fname = self.restore.create_filter_file()
        tables = None
        with open(fname) as fd:
            contents = fd.read()
            tables = contents.splitlines()
        self.assertEquals(tables,self.restore.restore_tables)
        os.remove(fname)

    @patch('gppylib.operations.restore.get_lines_from_file', return_value = ['public.t1', 'public.t2', 'public.t3'])
    @patch('os.path.isfile', return_value = True)
    def test_get_restore_tables_from_table_file_00(self, mock1, mock2):
        table_file = '/foo'
        expected_result = ['public.t1', 'public.t2', 'public.t3']
        result = get_restore_tables_from_table_file(table_file)
        self.assertEqual(expected_result, result)

    @patch('os.path.isfile', return_value = False)
    def test_get_restore_tables_from_table_file_01(self, mock):
        table_file = '/foo'
        expected_result = ['public.t1', 'public.t2', 'public.t3']
        with self.assertRaisesRegexp(Exception, 'Table file does not exist'):
            result = get_restore_tables_from_table_file(table_file)

    def test_validate_tablenames_00(self):
        table_list = ['publicao1', 'public.ao2']
        with self.assertRaisesRegexp(Exception, 'No schema name supplied'):
            validate_tablenames(table_list)

    def test_validate_tablenames_01(self):
        table_list = ['public.ao1', 'public.ao2']
        validate_tablenames(table_list)

    def test_validate_tablenames_02(self):
        table_list = []
        validate_tablenames(table_list)

    def test_validate_tablenames_03(self):
        table_list = ['public.ao1', 'public.ao1']
        resolved_list = validate_tablenames(table_list)
        self.assertEqual(resolved_list, ['public.ao1'])

    def test_validate_tablenames_04(self):
        table_list = ['public.*', 'public.ao1']
        resolved_list = validate_tablenames(table_list)
        self.assertEqual(resolved_list, ['public.*'])

    def test_validate_tablenames_05(self):
        table_list = ['public.*', 'other.*']
        resolved_list = validate_tablenames(table_list)
        self.assertEqual(resolved_list, ['public.*', 'other.*'])

    def test_get_restore_table_list_00(self):
        table_list = ['public.ao_table', 'public.ao_table2', 'public.co_table', 'public.heap_table']
        restore_tables = ['public.ao_table2', 'public.co_table']
        result = get_restore_table_list(table_list, restore_tables)
        with open(result) as fd:
            for line in fd:
                self.assertTrue(line.strip() in restore_tables)

    def test_get_restore_table_list_01(self):
        table_list = ['public.ao_table', 'public.ao_table2', 'public.co_table', 'public.heap_table']
        restore_tables = None
        result = get_restore_table_list(table_list, restore_tables)
        with open(result) as fd:
            for line in fd:
                self.assertTrue(line.strip() in table_list)

    def test_get_restore_table_list_02(self):
        table_list = ['public.ao_table', 'public.ao_table2', 'public.co_table', 'public.heap_table']
        restore_tables = ['public.ao_table2', 'public.co_table', 'public.ao_table3']
        result = get_restore_table_list(table_list, restore_tables)
        with open(result) as fd:
            for line in fd:
                self.assertTrue(line.strip() in restore_tables)

    def test_validate_restore_tables_list_00(self):
        plan_file_contents = [('20121212121213', 'public.t1'), ('20121212121212', 'public.t2,public.t3'), ('20121212121212', 'public.t4')]
        restore_tables = ['public.t1', 'public.t2']
        validate_restore_tables_list(plan_file_contents, restore_tables)

    def test_validate_restore_tables_list_01(self):
        plan_file_contents = [('20121212121213', 'public.t1'), ('20121212121212', 'public.t2,public.t3'), ('20121212121212', 'public.t4')]
        restore_tables = ['public.t5', 'public.t2']
        with self.assertRaisesRegexp(Exception, 'Invalid tables for -T option: The following tables were not found in plan file'):
            validate_restore_tables_list(plan_file_contents, restore_tables)

    def test_validate_restore_tables_list_02(self):
        plan_file_contents = [('20121212121213', 'public.t1'), ('20121212121212', 'public.t2,public.t3'), ('20121212121212', 'public.Ž')]
        restore_tables = ['public.t1', 'public.Áá']
        with self.assertRaisesRegexp(Exception, 'Invalid tables for -T option: The following tables were not found in plan file'):
            validate_restore_tables_list(plan_file_contents, restore_tables)

    def test_validate_restore_tables_list_03(self):
        plan_file_contents = [('20121212121213', 'public.t1'), ('20121212121212', 'public.t2,public.t3'), ('20121212121212', 'public.测试')]
        restore_tables = ['public.t1', 'public.测试']
        validate_restore_tables_list(plan_file_contents, restore_tables)

    def test_validate_restore_tables_list_04(self):
        plan_file_contents = [('20121212121213', 'public.t1'), ('20121212121212', 'public.t2,public.t3'), ('20121212121212', 'public.Ž')]
        restore_tables = ['public.t1', 'public.Ž']
        validate_restore_tables_list(plan_file_contents, restore_tables)

    def test_validate_restore_tables_list_05(self):
        plan_file_contents = [('20121212121213', 'public.t1'), ('20121212121212', 'public.t2,public.t3'), ('20121212121212', 'public.Áá')]
        restore_tables = ['public.t1', 'public.Áá']
        validate_restore_tables_list(plan_file_contents, restore_tables)

    @patch('gppylib.operations.unix.CheckFile.run', return_value=False)
    def test_restore_global_00(self, mock):
        restore_timestamp = '20121212121212'
        master_datadir = 'foo'
        backup_dir = None

        with self.assertRaisesRegexp(Exception, 'Unable to locate global file gp_global_1_1_20121212121212 in dump set'):
            self.restore._restore_global(restore_timestamp, master_datadir, backup_dir)

    @patch('os.path.exists', return_value=True)
    @patch('gppylib.commands.gp.Psql.run')
    def test_restore_global_01(self, mock1, mock2):
        restore_timestamp = '20121212121212'
        master_datadir = 'foo'
        backup_dir = None

        self.restore._restore_global(restore_timestamp, master_datadir, backup_dir) # should not error out

    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_00(self, m1, m2):
        conn = None
        schema = None
        table = None
        counter = 1
        batch_size = 1000
        update_ao_stat_func(conn, schema, table, counter, batch_size)

    @patch('pygresql.pgdb.pgdbCnx.commit')
    @patch('gppylib.operations.restore.execSQLForSingleton')
    def test_update_ao_stat_func_01(self, m1, m2):
        conn = None
        schema = None
        table = None
        counter = 999
        batch_size = 1000
        update_ao_stat_func(conn, schema, table, counter, batch_size)

    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_02(self, m1, m2):
        conn = None
        schema = None
        table = None
        counter = 1000
        batch_size = 1000
        with self.assertRaisesRegexp(AttributeError, "'NoneType' object has no attribute 'commit'"):
            update_ao_stat_func(conn, schema, table, counter, batch_size)

    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_03(self, m1, m2):
        conn = None
        schema = None
        table = None
        counter = 1001
        batch_size = 1000
        update_ao_stat_func(conn, schema, table, counter, batch_size)

    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_04(self, m1, m2):
        conn = None
        schema = None
        table = None
        counter = 2000
        batch_size = 1000
        with self.assertRaisesRegexp(AttributeError, "'NoneType' object has no attribute 'commit'"):
            update_ao_stat_func(conn, schema, table, counter, batch_size)


    @patch('gppylib.operations.restore.execute_sql', return_value=[['t1', 'public']])
    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.operations.restore.update_ao_stat_func')
    def test_update_ao_statistics_00(self, m1, m2, m3):
        port = 28888
        db = 'testdb'
        update_ao_statistics(port, db)

    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value=5)
    def test_check_gp_toolkit_true(self, m1, m2):
        restore_db = 'testdb'
        self.assertTrue(self.restore.check_gp_toolkit(restore_db))

    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value=0)
    def test_check_gp_toolkit_false(self, m1, m2):
        restore_db = 'testdb'
        self.assertFalse(self.restore.check_gp_toolkit(restore_db))

    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('gppylib.operations.restore.execSQL')
    def test_analyze_restore_tables_00(self, mock1, mock2, mock3):
        db_name = 'FOO'
        port = 1234
        restore_tables = ['public.t1', 'public.t2']
        restoredb = RestoreDatabase('20121219', False, True, False, 'FOO', None, 1234, 'db_dumps', '', False, False, None, None, False, None, None, None, None)
        restoredb._analyze_restore_tables(db_name, restore_tables)

    @patch('gppylib.operations.restore.execSQL', side_effect=Exception('analyze failed'))
    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    def test_analyze_restore_tables_01(self, mock1, mock2, mock3):
        db_name = 'FOO'
        port = 1234
        restore_tables = ['public.t1', 'public.t2']
        restoredb = RestoreDatabase('20121219', False, True, False, 'FOO', None, 1234, 'db_dumps', '', False, False, None, None, False, None, None, None, None)
        self.assertRaises(Exception, restoredb._analyze_restore_tables, db_name, restore_tables)

    @patch('gppylib.operations.backup_utils.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.DbURL', side_effect=Exception('Failed'))
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    def test_analyze_restore_tables_02(self, mock1, mock2, mock3):
        db_name = 'FOO'
        port = 1234
        restore_tables = ['public.t1', 'public.t2']
        restoredb = RestoreDatabase('20121219', False, True, False, 'FOO', None, 1234, 'db_dumps', '', False, False, None, None, False, None, None, None, None)
        self.assertRaises(Exception, restoredb._analyze_restore_tables, db_name, restore_tables)

    @patch('gppylib.operations.backup_utils.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect', side_effect=Exception('Failed'))
    def test_analyze_restore_tables_03(self, mock1, mock2, mock3):
        db_name = 'FOO'
        port = 1234
        restore_tables = ['public.t1', 'public.t2']
        restoredb = RestoreDatabase('20121219', False, True, False, 'FOO', None, 1234, 'db_dumps', '', False, False, None, None, False, None, None, None, None)
        self.assertRaises(Exception, restoredb._analyze_restore_tables, db_name, restore_tables)

    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('gppylib.operations.restore.execSQL')
    def test_analyze_restore_tables_04(self, mock1, mock2, mock3):
        db_name = 'FOO'
        port = 1234
        restore_tables = ['public.t%d' % i for i in range(3002)]
        expected_batch_count = 3
        restoredb = RestoreDatabase('20121219', False, True, False, 'FOO', None, 1234, 'db_dumps', '', False, False, None, None, False, None, None, None, None)
        batch_count = restoredb._analyze_restore_tables(db_name, restore_tables)
        self.assertEqual(batch_count, expected_batch_count)

class ValidateTimestampTestCase(unittest.TestCase):

    def setUp(self):
        self.validate_timestamp = ValidateTimestamp(candidate_timestamp='20140211111111',
                                                    master_datadir='/mdd',
                                                    backup_dir='/backup_dir',
                                                    dump_dir='/db_dumps',
                                                    dump_prefix='',
                                                    netbackup_service_host=None,
                                                    ddboost=False)

    @patch('os.path.exists', side_effect=[True, False])
    def test_validate_compressed_file_with_compression_exists(self, mock):
        compressed_file = 'compressed_file.gz'
        self.assertTrue(self.validate_timestamp.validate_compressed_file(compressed_file))

    @patch('os.path.exists', side_effect=[False, False])
    def test_validate_compressed_file_with_compression_doesnt_exists(self, mock):
        compressed_file = 'compressed_file.gz'
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, 'Unable to find compressed_file or compressed_file.gz'):
            self.validate_timestamp.validate_compressed_file(compressed_file)

    @patch('os.path.exists', side_effect=[False, True])
    def test_validate_compressed_file_without_compression_exists(self, mock):
        compressed_file = 'compressed_file.gz'
        self.assertFalse(self.validate_timestamp.validate_compressed_file(compressed_file))

    @patch('os.path.exists', side_effect=[False, False])
    def test_validate_compressed_file_without_compression_doesnt_exist(self, mock):
        compressed_file = 'compressed_file.gz'
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, 'Unable to find compressed_file or compressed_file.gz'):
            self.validate_timestamp.validate_compressed_file(compressed_file)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_00(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_01(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_02(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_03(self, mock1):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_04(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_05(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = None
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_06(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048

        restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_07(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024

        restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_08(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        restore_state_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_00(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_01(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_02(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_03(self, mock1):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_04(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_05(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = None
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_06(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024

        restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_07(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048

        restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_report_file_with_nbu_08(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        restore_report_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_00(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_01(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_02(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_03(self, mock1):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_04(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_05(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = None
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_06(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024

        restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_07(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048

        restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_cdatabase_file_with_nbu_08(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        restore_cdatabase_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_00(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_01(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_02(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_03(self, mock1):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_04(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_05(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = None
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_06(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024

        restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_07(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048

        restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_global_file_with_nbu_08(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        restore_global_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_00(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_01(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_02(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_03(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_04(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_05(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = None
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_06(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        master_port = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        with self.assertRaisesRegexp(Exception, 'Master port is None.'):
            restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_07(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_08(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    @patch('gppylib.operations.restore.generate_segment_config_filename')
    def test_restore_config_files_with_nbu_09(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        restore_config_files_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_00(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_01(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_02(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_03(self, mock1):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_04(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_05(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = None
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_06(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024

        restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_07(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048

        restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_partition_list_file_with_nbu_08(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141400002014"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        restore_partition_list_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = None
        netbackup_service_host = "mdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20140808000000"
        netbackup_service_host = None
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_06(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024

        restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_07(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = 2048

        restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value='20140707000000')
    def test_restore_increments_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental_with_nbu', return_value=None)
    def test_restore_increments_file_with_nbu_09(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20140808000000"
        netbackup_service_host = "mdw"
        netbackup_block_size = 4096

        with self.assertRaisesRegexp(Exception, 'Unable to locate full timestamp for given incremental timestamp "20140808000000" using NetBackup'):
            restore_increments_file_with_nbu(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.restore.get_backup_directory', return_value="/data")
    @patch('gppylib.operations.restore.generate_master_config_filename', return_value="gp_master_config_20141200002014.tar")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=True)
    def test_config_files_dumped_00(self, mock1, mock2, mock3):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertTrue(config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.get_backup_directory', return_value="/data")
    @patch('gppylib.operations.restore.generate_master_config_filename', return_value="gp_master_config_20141200002014.tar")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=True)
    def test_config_files_dumped_01(self, mock1, mock2, mock3):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertTrue(config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.get_backup_directory', return_value="/datadomain")
    @patch('gppylib.operations.restore.generate_master_config_filename', return_value="gp_master_config_20141200002014.tar")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=True)
    def test_config_files_dumped_02(self, mock1, mock2, mock3):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertTrue(config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu')
    def test_config_files_dumped_03(self, mock1, mock2, mock3):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu')
    def test_config_files_dumped_04(self, mock1, mock2, mock3):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = None
        netbackup_service_host = "mdw"

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host)

    @patch('gppylib.operations.restore.get_backup_directory')
    @patch('gppylib.operations.restore.generate_master_config_filename')
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu')
    def test_config_files_dumped_05(self, mock1, mock2, mock3):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host)

    @patch('gppylib.operations.restore.get_backup_directory', return_value="/datadomain")
    @patch('gppylib.operations.restore.generate_master_config_filename', return_value="gp_master_config_20141200002014.tar")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=False)
    def test_config_files_dumped_06(self, mock1, mock2, mock3):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertFalse(config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.get_backup_directory', return_value="/datadomain")
    @patch('gppylib.operations.restore.generate_master_config_filename', return_value="gp_master_config_20141200002014.tar")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=False)
    def test_config_files_dumped_07(self, mock1, mock2, mock3):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertFalse(config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.get_backup_directory', return_value="/data")
    @patch('gppylib.operations.restore.generate_master_config_filename', return_value="gp_master_config_20141200002014.tar")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=False)
    def test_config_files_dumped_08(self, mock1, mock2, mock3):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertFalse(config_files_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.generate_global_filename', return_value="/data/gp_global_1_1_20141200002014")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=True)
    def test_global_file_dumped_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertTrue(global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.generate_global_filename', return_value="/data/gp_global_1_1_20141200002014")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=True)
    def test_global_file_dumped_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertTrue(global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.generate_global_filename', return_value="/datadomain/gp_global_1_1_20141200002014")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=True)
    def test_global_file_dumped_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertTrue(global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.generate_global_filename')
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu')
    def test_global_file_dumped_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host)

    @patch('gppylib.operations.restore.generate_global_filename')
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu')
    def test_global_file_dumped_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = None
        netbackup_service_host = "mdw"

        with self.assertRaisesRegexp(Exception, 'Restore timestamp is None.'):
            global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host)

    @patch('gppylib.operations.restore.generate_global_filename')
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu')
    def test_global_file_dumped_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = None

        with self.assertRaisesRegexp(Exception, 'Netbackup service hostname is None.'):
            global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host)

    @patch('gppylib.operations.restore.generate_global_filename', return_value="/datadomain/gp_global_1_1_20141200002014")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=False)
    def test_global_file_dumped_06(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertFalse(global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.generate_global_filename', return_value="/datadomain/gp_global_1_1_20141200002014")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=False)
    def test_global_file_dumped_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertFalse(global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

    @patch('gppylib.operations.restore.generate_global_filename', return_value="/data/gp_global_1_1_20141200002014")
    @patch('gppylib.operations.restore.check_file_dumped_with_nbu', return_value=False)
    def test_global_file_dumped_08(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        restore_timestamp = "20141200002014"
        netbackup_service_host = "mdw"

        self.assertFalse(global_file_dumped(master_datadir, backup_dir, self.validate_timestamp.dump_dir, self.validate_timestamp.dump_prefix, restore_timestamp, netbackup_service_host))

if __name__ == '__main__':
    unittest.main()
