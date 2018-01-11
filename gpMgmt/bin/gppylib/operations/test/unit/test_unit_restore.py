#!/usr/bin/env python
# coding: utf-8
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#

import sys
import unittest
import tempfile, os, shutil
from gppylib.commands.base import CommandResult, Command, ExecutionError
from gppylib.operations.backup_utils import *
from gppylib.operations.restore import *
from gppylib.operations.restore import _build_gpdbrestore_cmd_line
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from mock import patch, MagicMock, Mock, mock_open, call, ANY
from . import setup_fake_gparray

class RestoreTestCase(unittest.TestCase):

    def setUp(self):
        context = Context()
        with patch('gppylib.gparray.GpArray.initFromCatalog', return_value=setup_fake_gparray()):
            context = Context()
        context.target_db='testdb'
        context.include_dump_tables_file='/tmp/table_list.txt'
        context.master_datadir='/data/master/p1'
        context.backup_dir=None
        context.batch_default=None
        context.timestamp = '20160101010101'
        context.no_analyze = True
        context.drop_db = True
        context.master_port = 5432

        self.context = context
        self.restore = RestoreDatabase(self.context)
        self.validate_timestamp = ValidateTimestamp(self.context)

    def test_GetDbName_default(self):
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

    def test_GetDbName_line_check(self):
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

    def test_GetDbName_no_name(self):
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
    def test_multitry_createdb_create_fails(self, mock1, mock2):
        self.assertRaises(ExceptionNoStackTraceNeeded, self.restore._multitry_createdb)

    @patch('gppylib.operations.restore.RestoreDatabase._process_createdb')
    def test_multitry_createdb_default(self, mock):
        self.restore._multitry_createdb()

    @patch('gppylib.operations.restore.get_partition_list', return_value=[('public', 't1'), ('public', 't2'), ('public', 't3')])
    @patch('gppylib.operations.restore.get_incremental_restore_timestamps', return_value=['20160101010101', '20160101010111'])
    @patch('gppylib.operations.restore.get_dirty_table_file_contents', return_value=['public.t1', 'public.t2'])
    def test_create_restore_plan_default(self, mock1, mock2, mock3):
        expected = ["20160101010111:", "20160101010101:public.t1,public.t2", "20160101000000:public.t3"]
        self.context.full_dump_timestamp = '20160101000000'
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            plan_file = create_restore_plan(self.context)
            result = m()
            self.assertEqual(len(expected), len(result.write.call_args_list))
            for i in range(len(expected)):
                self.assertEqual(call(expected[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.restore.get_incremental_restore_timestamps', return_value=['20160101010101', '20160101010111'])
    @patch('gppylib.operations.restore.get_dirty_table_file_contents', return_value=['public.t1', 'public.t2'])
    def test_create_restore_plan_empty_list(self, mock1, mock2):
        expected = ["20160101010111:", "20160101010101:", "20160101000000:"]
        self.context.full_dump_timestamp = '20160101000000'
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            plan_file = create_restore_plan(self.context)
            result = m()
            self.assertEqual(len(expected), len(result.write.call_args_list))
            for i in range(len(expected)):
                self.assertEqual(call(expected[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.restore.get_partition_list', return_value=[])
    @patch('gppylib.operations.restore.get_full_timestamp_for_incremental', return_value='20120101000000')
    @patch('gppylib.operations.restore.get_incremental_restore_timestamps', return_value=['20160101010101', '20160101010111'])
    @patch('gppylib.operations.restore.get_dirty_table_file_contents', return_value=['public.t1', 'public.t2'])
    @patch('gppylib.operations.restore.create_plan_file_contents')
    def test_create_restore_plan_empty_list_with_nbu(self, mock1, mock2, mock3, mock4, mock5):
        self.context.netbackup_service_host = 'mdw'
        self.context.netbackup_block_size = '1024'
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            plan_file = create_restore_plan(self.context)
            result = m()
            self.assertEqual(len(result.write.call_args_list), 0)

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20160101010110', '20160101010109', '20160101010108', '20160101010107', '20160101010106', '20160101010105', '20160101010104', '20160101010103', '20160101010102', '20160101010101'])
    def test_get_incremental_restore_timestamps_midway(self, mock):
        self.context.full_dump_timestamp = '20160101010101'
        self.context.timestamp = '20160101010105'
        increments = get_incremental_restore_timestamps(self.context)
        self.assertEqual(increments, ['20160101010105', '20160101010104', '20160101010103', '20160101010102', '20160101010101'])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20160101010110', '20160101010109', '20160101010108', '20160101010107', '20160101010106', '20160101010105', '20160101010104', '20160101010103', '20160101010102', '20160101010101'])
    def test_get_incremental_restore_timestamps_latest(self, mock):
        self.context.full_dump_timestamp = '20160101010101'
        self.context.timestamp = '20160101010110'
        increments = get_incremental_restore_timestamps(self.context)
        self.assertEqual(increments, ['20160101010110', '20160101010109', '20160101010108', '20160101010107', '20160101010106', '20160101010105', '20160101010104', '20160101010103', '20160101010102', '20160101010101'])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=[])
    def test_get_incremental_restore_timestamps_earliest(self, mock):
        self.context.full_dump_timestamp = '20160101010101'
        self.context.timestamp = '20160101010100'
        increments = get_incremental_restore_timestamps(self.context)
        self.assertEqual(increments, [])

    @patch('gppylib.operations.restore.get_lines_from_file', side_effect=[['public.t1'], ['public.t1', 'public.t2', 'public.t3'], ['public.t2', 'public.t4']])
    def test_create_plan_file_contents_with_file(self, mock):
        table_set_from_metadata_file = ['public.t1', 'public.t2', 'public.t3', 'public.t4']
        incremental_restore_timestamps = ['20160101010113', '20160101010101', '20160101010111']
        latest_full_timestamp = '20160101010110'
        expected_output = {'20160101010113': ['public.t1'], '20160101010101': ['public.t2', 'public.t3'], '20160101010111': ['public.t4'], '20160101010110': []}
        file_contents = create_plan_file_contents(self.context, table_set_from_metadata_file, incremental_restore_timestamps, latest_full_timestamp)
        self.assertEqual(file_contents, expected_output)

    def test_create_plan_file_contents_no_file(self):
        table_set_from_metadata_file = ['public.t1', 'public.t2', 'public.t3', 'public.t4']
        incremental_restore_timestamps = []
        latest_full_timestamp = '20160101010110'
        expected_output = {'20160101010110': ['public.t1', 'public.t2', 'public.t3', 'public.t4']}
        file_contents = create_plan_file_contents(self.context, table_set_from_metadata_file, incremental_restore_timestamps, latest_full_timestamp)
        self.assertEqual(file_contents, expected_output)

    @patch('gppylib.operations.restore.get_lines_from_file', side_effect=[['public.t1'], ['public.t1', 'public.t2', 'public.t3'], ['public.t2', 'public.t4']])
    def test_create_plan_file_contents_no_metadata(self, mock):
        table_set_from_metadata_file = []
        incremental_restore_timestamps = ['20160101010113', '20160101010101', '20160101010111']
        latest_full_timestamp = '20160101010110'
        expected_output = {'20160101010101': [], '20160101010113': [], '20160101010111': [], '20160101010110': []}
        file_contents = create_plan_file_contents(self.context, table_set_from_metadata_file, incremental_restore_timestamps, latest_full_timestamp)
        self.assertEqual(file_contents, expected_output)

    @patch('gppylib.operations.restore.get_lines_from_file', side_effect=[['public.t1'], ['public.t1', 'public.t2', 'public.t3'], ['public.t2', 'public.t4']])
    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_create_plan_file_contents_with_nbu(self, mock1, mock2):
        self.context.netbackup_service_host = 'mdw'
        self.context.netbackup_block_size = '1024'
        table_set_from_metadata_file = []
        incremental_restore_timestamps = ['20160101010113', '20160101010101', '20160101010111']
        latest_full_timestamp = '20160101010110'
        expected_output = {'20160101010101': [], '20160101010113': [], '20160101010111': [], '20160101010110': []}
        file_contents = create_plan_file_contents(self.context, table_set_from_metadata_file, incremental_restore_timestamps, latest_full_timestamp)
        self.assertEqual(file_contents, expected_output)

    @patch('gppylib.operations.restore.write_lines_to_file')
    def test_write_to_plan_file_default(self, mock1):
        plan_file = 'blah'
        plan_file_contents = {'20160101010113': ['public.t1'],
                              '20160101010101': ['public.t2', 'public.t3'],
                              '20160101010111': ['public.t4']}

        expected_output = ['20160101010113:public.t1',
                           '20160101010111:public.t4',
                           '20160101010101:public.t2,public.t3']

        file_contents = write_to_plan_file(plan_file_contents, plan_file)
        self.assertEqual(expected_output, file_contents)

    @patch('gppylib.operations.restore.write_lines_to_file')
    def test_write_to_plan_file_empty_list(self, mock1):
        plan_file = 'blah'
        plan_file_contents = {}
        expected_output = []
        file_contents = write_to_plan_file(plan_file_contents, plan_file)
        self.assertEqual(expected_output, file_contents)

    @patch('gppylib.operations.restore.write_lines_to_file')
    def test_write_to_plan_file_no_plan_file(self, mock1):
        plan_file = None
        plan_file_contents = {}
        with self.assertRaisesRegexp(Exception, 'Invalid plan file .*'):
            write_to_plan_file(plan_file_contents, plan_file)

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['public.t1', 'public.t2'])
    def test_get_partition_list_default(self, mock):
        partition_list = get_partition_list(self.context)
        self.assertEqual(partition_list, [('public', 't1'), ('public', 't2')])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=[])
    def test_get_partition_list_no_partitions(self, mock):
        partition_list = get_partition_list(self.context)
        self.assertEqual(partition_list, [])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Incremental'])
    @patch('os.path.isfile', return_value=True)
    def test_is_incremental_restore_default(self, mock1, mock2):
        self.assertTrue(is_incremental_restore(self.context))

    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=True)
    @patch('os.path.isfile', return_value=True)
    def test_is_incremental_restore_bypass_file_incremental(self, mock1, mock2, mock3):
        self.assertTrue(is_incremental_restore(self.context))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Full'])
    def test_is_incremental_restore_full_backup(self, mock1, mock2):
        self.assertFalse(is_incremental_restore(self.context))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=False)
    def test_is_incremental_restore_bypass_file_full(self, mock1, mock2, mock3):
        self.assertFalse(is_incremental_restore(self.context))

    @patch('os.path.isfile', return_value=False)
    def test_is_incremental_restore_no_file(self, mock1):
        self.assertFalse(is_incremental_restore(self.context))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Full'])
    @patch('os.path.isfile', return_value=True)
    def test_is_full_restore_default(self, mock1, mock2, mock3):
        self.assertTrue(is_full_restore(self.context))

    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=True)
    @patch('os.path.isfile', return_value=True)
    def test_is_full_restore_bypass_file_full(self, mock1, mock2, mock3):
        self.assertTrue(is_full_restore(self.context))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['Backup Type: Incremental'])
    def test_is_full_restore_incremental(self, mock1, mock2):
        self.assertFalse(is_full_restore(self.context))

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.get_lines_from_file')
    @patch('gppylib.operations.restore.check_backup_type', return_value=False)
    def test_is_full_restore_bypass_file_incremental(self, mock1, mock2, mock3):
        self.assertFalse(is_full_restore(self.context))

    @patch('os.path.isfile', return_value=False)
    def test_is_full_restore_no_file(self, mock1):
        filename = self.context.generate_filename("report")
        with self.assertRaisesRegexp(Exception, 'Report file %s does not exist' % filename):
            is_full_restore(self.context)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_schema_only_restore_string_default(self, mock1, mock2):
        table_filter_file = None
        full_restore_with_filter = False
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" -s %s' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_schema_only_restore_string_no_compression(self, mock1, mock2):
        self.context.compress = False
        table_filter_file = None
        full_restore_with_filter = False
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p -d "testdb" -s %s' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=True)
    def test_create_schema_only_restore_string_backup_dir(self, mock1, mock2, mock3):
        table_filter_file = None
        full_restore_with_filter = False
        self.context.report_status_dir = "/data/master/p1/db_dumps/20160101"
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/data/master/p1/db_dumps/20160101 --status=/data/master/p1/db_dumps/20160101 --gp-c -d "testdb" -s %s' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=False)
    def test_create_schema_only_restore_string_prefix(self, mock1, mock2, mock3):
        self.context.dump_prefix = 'bar_'
        table_filter_file = 'filter_file1'
        metadata_file = self.context.generate_filename("metadata")
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --prefix=bar_ --gp-f=%s --gp-c -d "testdb" -s %s' % (table_filter_file, metadata_file)

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=False)
    def test_create_schema_only_restore_string_no_filter_file(self, mock1, mock2, mock3):
        table_filter_file = None
        metadata_file = self.context.generate_filename("metadata")
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" -s %s' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_schema_only_restore_string_different_status_dir(self, mock1, mock2):
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = False
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" -s %s' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_schema_only_restore_string_status_dir_with_filter(self, mock1, mock2):
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = True
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" -s %s -P' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_schema_only_restore_string_with_nbu(self, mock1, mock2):
        table_filter_file = None
        full_restore_with_filter = False
        self.context.netbackup_service_host = "mdw"
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" --netbackup-service-host=mdw -s %s' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_schema_only_restore_string_with_ddboost(self, mock1, mock2):
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = True
        self.context.ddboost = True
        self.context.dump_dir = '/backup/DCA-35'
        metadata_file = self.context.generate_filename("metadata")
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/backup/DCA-35/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" --ddboost -s %s -P' % metadata_file

        restore_line = self.restore.create_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_post_data_schema_only_restore_string_default(self, mock1, mock2):
        table_filter_file = None
        full_restore_with_filter = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" -P'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=True)
    def test_create_post_data_schema_only_restore_string_no_filter(self, mock1, mock2, mock3):
        table_filter_file = None
        full_restore_with_filter = False
        self.context.report_status_dir="/data/master/p1/db_dumps/20160101"
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/data/master/p1/db_dumps/20160101 --status=/data/master/p1/db_dumps/20160101 --gp-c -d "testdb"'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=False)
    def test_create_post_data_schema_only_restore_string_with_prefix(self, mock1, mock2, mock3):
        self.context.dump_prefix = 'bar_'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --prefix=bar_ --gp-c -d "testdb"'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)


    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=False)
    def test_create_post_data_schema_only_restore_string_with_prefix_and_filter(self, mock1, mock2, mock3):
        self.context.dump_prefix = 'bar_'
        table_filter_file = 'filter_file1'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --prefix=bar_ --gp-f=%s --gp-c -d "testdb"' % (table_filter_file)

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=False)
    def test_create_post_data_schema_only_restore_string_no_backup_dir(self, mock1, mock2, mock3):
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb"'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_post_data_schema_only_restore_string_different_status_dir(self, mock1, mock2):
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb"'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_post_data_schema_only_restore_string_status_dir_and_filter(self, mock1, mock2):
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" -P'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_post_data_schema_only_restore_string_with_ddboost(self, mock1, mock2):
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = True
        self.context.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" --ddboost -P'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_post_data_schema_only_restore_string_with_nbu(self, mock1, mock2):
        table_filter_file = None
        full_restore_with_filter = True
        self.context.backup_dir = None
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = 1024
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" --netbackup-service-host=mdw --netbackup-block-size=1024 -P'

        restore_line = self.restore.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_default(self, mock1, mock2):
        ts = '20160101010101'
        self.context.backup_dir = None
        expected_output = 'gpdbrestore -t 20160101010101 --table-file foo -a -v --noplan --noanalyze --noaostats --no-validate-table-name'
        restore_line = _build_gpdbrestore_cmd_line(self.context, ts, 'foo')
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_backup_dir(self, mock1, mock2):
        ts = '20160101010101'
        self.context.backup_dir = '/tmp'
        expected_output = 'gpdbrestore -t 20160101010101 --table-file foo -a -v --noplan --noanalyze --noaostats --no-validate-table-name -u /tmp'
        restore_line = _build_gpdbrestore_cmd_line(self.context, ts, 'foo')
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_report_status_dir(self, mock1, mock2):
        ts = '20160101010101'
        self.context.backup_dir = None
        self.context.report_status_dir = '/tmp'
        expected_output = 'gpdbrestore -t 20160101010101 --table-file foo -a -v --noplan --noanalyze --noaostats --no-validate-table-name --report-status-dir=/tmp'
        restore_line = _build_gpdbrestore_cmd_line(self.context, ts, 'foo')
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_redirected_restore(self, mock1, mock2):
        ts = '20160101010101'
        self.context.backup_dir = None
        self.context.redirected_restore_db = "redb"
        expected_output = 'gpdbrestore -t 20160101010101 --table-file foo -a -v --noplan --noanalyze --noaostats --no-validate-table-name --redirect=redb'
        restore_line = _build_gpdbrestore_cmd_line(self.context, ts, 'foo')
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_build_gpdbrestore_cmd_line_with_ddboost(self, mock1, mock2):
        ts = '20160101010101'
        self.context.backup_dir = None
        self.context.ddboost = True
        self.context.report_status_dir = '/tmp'
        expected_output = 'gpdbrestore -t 20160101010101 --table-file foo -a -v --noplan --noanalyze --noaostats --no-validate-table-name --report-status-dir=/tmp --ddboost'
        ddboost = True
        restore_line = _build_gpdbrestore_cmd_line(self.context, ts, 'foo')
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.dbconn.DbURL')
    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.operations.restore.execSQL')
    @patch('gppylib.operations.restore.RestoreDatabase.get_full_tables_in_schema', return_value=['"public"."tablename1"', '"public"."tablename2"', '"public"."tablename3"'])
    def test_truncate_restore_tables_restore_schemas(self, mock1, mock2, mock3, mock4):
        self.context.restore_schemas = ['public']
        self.restore.truncate_restore_tables()
        calls = [call(ANY,'Truncate "public"."tablename1"'), call(ANY,'Truncate "public"."tablename2"'), call(ANY,'Truncate "public"."tablename3"')]
        mock2.assert_has_calls(calls)

    @patch('gppylib.operations.restore.dbconn.DbURL')
    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.operations.restore.escape_string', side_effect=['public', 'ao1', 'testschema', 'heap1'])
    @patch('gppylib.operations.restore.execSQL')
    @patch('gppylib.operations.restore.execSQLForSingleton', return_value='t')
    def test_truncate_restore_tables_restore_tables(self, mock1, mock2, mock3, mock4, mock5):
        self.context.restore_tables = ['public.ao1', 'testschema.heap1']
        self.restore.truncate_restore_tables()
        calls = [call(ANY,'Truncate "public"."ao1"'), call(ANY,'Truncate "testschema"."heap1"')]
        mock2.assert_has_calls(calls)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_no_filter_file(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_default(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = '/tmp/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-f=/tmp/foo --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_with_ddboost(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = None
        full_restore_with_filter = False
        self.context.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" --ddboost -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_different_status_dir(self, mock1, mock2):
        self.context.no_plan = True
        self.context.report_status_dir = '/tmp'
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_no_filter(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = None
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_no_filter_file(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = '/tmp/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-f=/tmp/foo --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_ddboost_and_prefix(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = None
        self.context.dump_prefix = 'bar_'
        full_restore_with_filter = False
        self.context.ddboost = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --prefix=bar_ --gp-c -d "testdb" --ddboost -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    @patch('gppylib.operations.backup_utils.Context.backup_dir_is_writable', return_value=True)
    def test_create_restore_string_backup_dir(self, mock1, mock2, mock3):
        self.context.no_plan = True
        table_filter_file = None
        self.context.backup_dir = '/tmp'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/tmp/db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp/db_dumps/20160101 --status=/tmp/db_dumps/20160101 --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_no_ao_stats(self, mock1, mock2):
        self.context.no_plan = True
        self.context.no_ao_stats = True
        table_filter_file = None
        self.context.report_status_dir = '/tmp'
        self.context.backup_dir = '/foo'
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" -a --gp-nostats'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_with_plan(self, mock1, mock2):
        table_filter_file = None
        self.context.report_status_dir = '/tmp'
        self.context.backup_dir = '/foo'
        full_restore_with_filter = True
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_with_nbu(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = None
        self.context.report_status_dir = '/tmp'
        self.context.backup_dir = '/foo'
        self.context.netbackup_service_host = "mdw"
        full_restore_with_filter = False
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=/foo/db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-r=/tmp --status=/tmp --gp-c -d "testdb" --netbackup-service-host=mdw -a'

        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter)
        self.assertEqual(restore_line, expected_output)

    # Test to verify the command line for gp_restore
    @patch('gppylib.operations.restore.socket.gethostname', return_value='host')
    @patch('gppylib.operations.restore.getpass.getuser', return_value='user')
    def test_create_restore_string_change_schema(self, mock1, mock2):
        self.context.no_plan = True
        table_filter_file = None
        full_restore_with_filter = False
        change_schema_file = 'newschema'
        expected_output = 'gp_restore -i -h host -p 5432 -U user --gp-d=db_dumps/20160101 --gp-i --gp-k=20160101010101 --gp-l=p --gp-c -d "testdb" --change-schema-file=newschema -a'
        restore_line = self.restore.create_standard_restore_string(table_filter_file, full_restore_with_filter, change_schema_file)
        self.assertEqual(restore_line, expected_output)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo')
    def test_get_plan_file_contents_no_file(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Plan file foo does not exist'):
            get_plan_file_contents(self.context)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=[])
    @patch('os.path.isfile', return_value=True)
    def test_get_plan_file_contents_empty_file(self, mock1, mock2, mock3):
        with self.assertRaisesRegexp(Exception, 'Plan file foo has no contents'):
            get_plan_file_contents(self.context)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20160101010101:t1,t2', '20160101010111:t3,t4', '20160101121210:t5,t6,t7'])
    @patch('os.path.isfile', return_value=True)
    def test_get_plan_file_contents_default(self, mock1, mock2, mock3):
        expected_output = [('20160101010101','t1,t2'), ('20160101010111','t3,t4'), ('20160101121210','t5,t6,t7')]
        output = get_plan_file_contents(self.context)
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo')
    @patch('gppylib.operations.restore.get_lines_from_file', return_value=['20160101010101:', '20160101010111', '20160101121210:'])
    @patch('os.path.isfile', return_value=True)
    def test_get_plan_file_contents_invalid_format(self, mock1, mock2, mock3):
        with self.assertRaisesRegexp(Exception, 'Invalid plan file format'):
            get_plan_file_contents(self.context)

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20160101010101', 't1,t2'), ('20160101010111', 't3,t4'), ('20160101121210', 't5,t6,t7')])
    @patch('gppylib.operations.restore.Command.run')
    @patch('gppylib.operations.restore.update_ao_statistics')
    def test_restore_incremental_data_only_default(self, mock1, mock2, mock3):
        results = self.restore.restore_incremental_data_only()
        self.assertTrue(results)

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20160101010101', ''), ('20160101010111', ''), ('20160101121210', '')])
    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.restore.update_ao_statistics')
    def test_restore_incremental_data_only_no_tables(self, mock1, mock2, mock3):
        with self.assertRaisesRegexp(Exception, 'There were no tables to restore. Check the plan file contents for restore timestamp 20160101010101'):
            self.restore.restore_incremental_data_only()

    @patch('gppylib.operations.restore.get_plan_file_contents', return_value=[('20160101010101', 't1,t2'), ('20160101010111', 't3,t4'), ('20160101121210', 't5,t6,t7')])
    @patch('gppylib.operations.restore.Command.run', side_effect=Exception('Error executing gpdbrestore'))
    @patch('gppylib.operations.restore.update_ao_statistics')
    def test_restore_incremental_data_only_error(self, mock1, mock2, mock3):
        with self.assertRaisesRegexp(Exception, 'Error executing gpdbrestore'):
            self.restore.restore_incremental_data_only()

    def test_create_filter_file_no_tables(self):
        self.context.restore_tables = None
        self.assertEquals(self.restore.create_filter_file(), None)

    @patch('gppylib.operations.restore.get_all_segment_addresses', return_value=['host1'])
    @patch('gppylib.operations.restore.scp_file_to_hosts')
    def test_create_filter_file_default(self, m1, m2):
        self.context.restore_tables = ['public.ao1', 'testschema.heap1']
        m = mock_open()
        with patch('tempfile.NamedTemporaryFile', m, create=True):
            fname = self.restore.create_filter_file()
            result = m()
            self.assertEqual(len(self.context.restore_tables), len(result.write.call_args_list))
            for i in range(len(self.context.restore_tables)):
                self.assertEqual(call(self.context.restore_tables[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.restore.get_lines_from_file', return_value = ['public.t1', 'public.t2', 'public.t3'])
    @patch('os.path.isfile', return_value = True)
    def test_get_restore_tables_from_table_file_default(self, mock1, mock2):
        table_file = '/foo'
        expected_result = ['public.t1', 'public.t2', 'public.t3']
        result = get_restore_tables_from_table_file(table_file)
        self.assertEqual(expected_result, result)

    @patch('os.path.isfile', return_value = False)
    def test_get_restore_tables_from_table_file_no_file(self, mock):
        table_file = '/foo'
        expected_result = ['public.t1', 'public.t2', 'public.t3']
        with self.assertRaisesRegexp(Exception, 'Table file does not exist'):
            result = get_restore_tables_from_table_file(table_file)

    def test_check_table_name_format_and_duplicate_missing_schema(self):
        table_list = ['publicao1', 'public.ao2']
        with self.assertRaisesRegexp(Exception, 'No schema name supplied'):
            check_table_name_format_and_duplicate(table_list, None)

    def test_check_table_name_format_and_duplicate_default(self):
        table_list = ['public.ao1', 'public.ao2']
        check_table_name_format_and_duplicate(table_list, [])

    def test_check_table_name_format_and_duplicate_no_tables(self):
        table_list = []
        schema_list = []
        check_table_name_format_and_duplicate(table_list, schema_list)

    def test_check_table_name_format_and_duplicate_duplicate_tables(self):
        table_list = ['public.ao1', 'public.ao1']
        resolved_list, _ = check_table_name_format_and_duplicate(table_list, [])
        self.assertEqual(resolved_list, ['public.ao1'])

    def test_check_table_name_format_and_duplicate_funny_chars(self):
        table_list = [' `"@#$%^&( )_|:;<>?/-+={}[]*1Aa . `"@#$%^&( )_|:;<>?/-+={}[]*1Aa ', 'schema.ao1']
        schema_list = ['schema']
        resolved_table_list, resolved_schema_list = check_table_name_format_and_duplicate(table_list, schema_list)
        self.assertEqual(resolved_table_list, [' `"@#$%^&( )_|:;<>?/-+={}[]*1Aa . `"@#$%^&( )_|:;<>?/-+={}[]*1Aa '])
        self.assertEqual(resolved_schema_list, ['schema'])

    def test_validate_tablenames_exist_in_dump_file_no_tables(self):
        dumped_tables = []
        table_list = ['schema.ao']
        with self.assertRaisesRegexp(Exception, 'No dumped tables to restore.'):
            validate_tablenames_exist_in_dump_file(table_list, dumped_tables)

    def test_validate_tablenames_exist_in_dump_file_one_table(self):
        dumped_tables = [('schema', 'ao', 'gpadmin')]
        table_list = ['schema.ao']
        validate_tablenames_exist_in_dump_file(table_list, dumped_tables)

    def test_validate_tablenames_exist_in_dump_file_nonexistent_table(self):
        dumped_tables = [('schema', 'ao', 'gpadmin')]
        table_list = ['schema.ao', 'schema.co']
        with self.assertRaisesRegexp(Exception, "Tables \['schema.co'\] not found in backup"):
            validate_tablenames_exist_in_dump_file(table_list, dumped_tables)

    def test_get_restore_table_list_default(self):
        table_list = ['public.ao_table', 'public.ao_table2', 'public.co_table', 'public.heap_table']
        restore_tables = ['public.ao_table2', 'public.co_table']
        m = mock_open()
        with patch('tempfile.NamedTemporaryFile', m, create=True):
            result = get_restore_table_list(table_list, restore_tables)
            result = m()
            self.assertEqual(len(restore_tables), len(result.write.call_args_list))
            for i in range(len(restore_tables)):
                self.assertEqual(call(restore_tables[i]+'\n'), result.write.call_args_list[i])

    def test_get_restore_table_list_no_restore_tables(self):
        table_list = ['public.ao_table', 'public.ao_table2', 'public.co_table', 'public.heap_table']
        restore_tables = None
        m = mock_open()
        with patch('tempfile.NamedTemporaryFile', m, create=True):
            result = get_restore_table_list(table_list, restore_tables)
            result = m()
            self.assertEqual(len(table_list), len(result.write.call_args_list))
            for i in range(len(table_list)):
                self.assertEqual(call(table_list[i]+'\n'), result.write.call_args_list[i])

    def test_get_restore_table_list_extra_restore_tables(self):
        table_list = ['public.ao_table', 'public.ao_table2', 'public.co_table', 'public.heap_table']
        restore_tables = ['public.ao_table2', 'public.co_table', 'public.ao_table3']
        expected = ['public.ao_table2', 'public.co_table']
        m = mock_open()
        with patch('tempfile.NamedTemporaryFile', m, create=True):
            result = get_restore_table_list(table_list, restore_tables)
            result = m()
            self.assertEqual(len(expected), len(result.write.call_args_list))
            for i in range(len(expected)):
                self.assertEqual(call(expected[i]+'\n'), result.write.call_args_list[i])

    def test_validate_restore_tables_list_default(self):
        plan_file_contents = [('20160101121213', 'public.t1'), ('20160101010101', 'public.t2,public.t3'), ('20160101010101', 'public.t4')]
        restore_tables = ['public.t1', 'public.t2']
        validate_restore_tables_list(plan_file_contents, restore_tables)

    def test_validate_restore_tables_list_invalid_tables(self):
        plan_file_contents = [('20160101121213', 'public.t1'), ('20160101010101', 'public.t2,public.t3'), ('20160101010101', 'public.t4')]
        restore_tables = ['public.t5', 'public.t2']
        with self.assertRaisesRegexp(Exception, 'Invalid tables for -T option: The following tables were not found in plan file'):
            validate_restore_tables_list(plan_file_contents, restore_tables)

    @patch('os.path.exists', return_value=False)
    def test_restore_global_no_file(self, mock):
        with self.assertRaisesRegexp(Exception, 'Unable to locate global file /data/master/p1/db_dumps/20160101/gp_global_-1_1_20160101010101 in dump set'):
            self.restore._restore_global(self.context)

    @patch('os.path.exists', return_value=True)
    @patch('gppylib.commands.gp.Psql.run')
    def test_restore_global_default(self, mock1, mock2):
        self.restore._restore_global(self.context) # should not error out

    @patch('gppylib.operations.restore.escape_string', return_value='schema.table')
    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_default(self, m1, m2, m3):
        conn = None
        ao_schema = 'schema'
        ao_table = 'table'
        counter = 1
        batch_size = 1000
        update_ao_stat_func(self.context, conn, ao_schema, ao_table, counter, batch_size)

    @patch('gppylib.operations.restore.escape_string', return_value='schema.table')
    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_near_batch_size(self, m1, m2, m3):
        conn = None
        ao_table = 'table'
        ao_schema = 'schema'
        counter = 999
        batch_size = 1000
        update_ao_stat_func(self.context, conn, ao_schema, ao_table, counter, batch_size)

    @patch('gppylib.operations.restore.escape_string', return_value='schema.table')
    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_equal_batch_size(self, m1, m2, m3):
        conn = None
        ao_table = 'table'
        ao_schema = 'schema'
        counter = 1000
        batch_size = 1000
        with self.assertRaisesRegexp(AttributeError, "'NoneType' object has no attribute 'commit'"):
            update_ao_stat_func(self.context, conn, ao_schema, ao_table, counter, batch_size)

    @patch('gppylib.operations.restore.escape_string', return_value='schema.table')
    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_over_batch_size(self, m1, m2, m3):
        conn = None
        ao_table = 'table'
        ao_schema = 'schema'
        counter = 1001
        batch_size = 1000
        update_ao_stat_func(self.context, conn, ao_schema, ao_table, counter, batch_size)

    @patch('gppylib.operations.restore.escape_string', return_value='schema.table')
    @patch('gppylib.operations.restore.execSQLForSingleton')
    @patch('pygresql.pgdb.pgdbCnx.commit')
    def test_update_ao_stat_func_double_batch_size(self, m1, m2, m3):
        conn = None
        ao_table = 'table'
        ao_schema = 'schema'
        counter = 2000
        batch_size = 1000
        with self.assertRaisesRegexp(AttributeError, "'NoneType' object has no attribute 'commit'"):
            update_ao_stat_func(self.context, conn, ao_schema, ao_table, counter, batch_size)

    @patch('gppylib.operations.restore.execute_sql', return_value=[['t1', 'public']])
    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.operations.restore.update_ao_stat_func')
    def test_update_ao_statistics_default(self, m1, m2, m3):
        restored_tables = []
        update_ao_statistics(self.context, restored_tables)

        update_ao_statistics(self.context, restored_tables=['public.t1'], restored_schema=[], restore_all=False)
        update_ao_statistics(self.context, restored_tables=[], restored_schema=['public'], restore_all=False)
        update_ao_statistics(self.context, restored_tables=[], restored_schema=[], restore_all=True)

    def test_generate_restored_tables_no_table(self):
        results = [['t1','public'], ['t2', 'public'], ['foo', 'bar']]

        tables = generate_restored_tables(results, restored_tables=[], restored_schema=[], restore_all=False)
        self.assertEqual(tables, set())

    def test_generate_restored_tables_specified_table(self):
        results = [['t1','public'], ['t2', 'public'], ['foo', 'bar']]

        tables = generate_restored_tables(results, restored_tables=['public.t1'], restored_schema=[], restore_all=False)
        self.assertEqual(tables, set([('public','t1')]))

    def test_generate_restored_tables_specified_schema(self):
        results = [['t1','public'], ['t2', 'public'], ['foo', 'bar']]

        tables = generate_restored_tables(results, restored_tables=[], restored_schema=['public'], restore_all=False)
        self.assertEqual(tables, set([('public','t1'), ('public', 't2')]))

    def test_generate_restored_tables_full_restore(self):
        results = [['t1','public'], ['t2', 'public'], ['foo', 'bar']]

        tables = generate_restored_tables(results, restored_tables=[], restored_schema=[], restore_all=True)
        self.assertEqual(tables, set([('public','t1'), ('public', 't2'), ('bar', 'foo')]))

    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value=5)
    def test_check_gp_toolkit_true(self, m1, m2):
        self.assertTrue(self.restore.check_gp_toolkit())

    @patch('gppylib.operations.restore.dbconn.connect')
    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value=0)
    def test_check_gp_toolkit_false(self, m1, m2):
        self.assertFalse(self.restore.check_gp_toolkit())

    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('gppylib.operations.restore.execSQL')
    def test_analyze_restore_tables_default(self, mock1, mock2, mock3):
        self.context.restore_tables = ['public.t1', 'public.t2']
        self.restore._analyze_restore_tables()

    @patch('gppylib.operations.restore.execSQL', side_effect=Exception('analyze failed'))
    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    def test_analyze_restore_tables_analyze_failed(self, mock1, mock2, mock3):
        self.context.restore_tables = ['public.t1', 'public.t2']
        self.assertRaises(Exception, self.restore._analyze_restore_tables)

    @patch('gppylib.operations.backup_utils.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.DbURL', side_effect=Exception('Failed'))
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    def test_analyze_restore_tables_connection_failed(self, mock1, mock2, mock3):
        self.context.restore_tables = ['public.t1', 'public.t2']
        self.assertRaises(Exception, self.restore._analyze_restore_tables)

    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('gppylib.operations.restore.execSQL')
    def test_analyze_restore_tables_three_batches(self, mock1, mock2, mock3):
        self.context.restore_tables = ['public.t%d' % i for i in range(3002)]
        expected_batch_count = 3
        batch_count = self.restore._analyze_restore_tables()
        self.assertEqual(batch_count, expected_batch_count)

    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('gppylib.operations.backup_utils.dbconn.execSQL')
    def test_analyze_restore_tables_change_schema(self, mock1, mock2, mock3):
        self.context.restore_tables = ['public.t1', 'public.t2']
        self.context.change_schema = 'newschema'
        self.restore._analyze_restore_tables()

    @patch('gppylib.operations.restore.execSQL', side_effect=Exception())
    @patch('gppylib.operations.backup_utils.dbconn.DbURL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    def test_analyze_restore_tables_execSQL_failed(self, mock1, mock2, mock3):
        self.context.target_db = 'db1'
        self.context.restore_tables = ['public.t1', 'public.t2']
        self.assertRaisesRegexp(Exception, 'Issue with \'ANALYZE\' of restored table \'"public"."t1"\' in \'db1\' database', self.restore._analyze_restore_tables)

    @patch('gppylib.operations.restore.restore_file_with_nbu')
    def test_restore_state_files_with_nbu_default(self, mock1):
        self.context.netbackup_service_host = "mdw"

        restore_state_files_with_nbu(self.context)
        self.assertEqual(mock1.call_count, 3)
        calls = ["ao", "co", "last_operation"]
        for i in range(len(mock1.call_args_list)):
            self.assertEqual(mock1.call_args_list[i], call(self.context, calls[i]))

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_default(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        cmdStr = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename /tmp/foo_schema > /tmp/foo_schema"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            restore_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("restoring metadata files to master", cmdStr)
            self.assertEqual(mock2.call_count, 1)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_no_filetype(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = 100
        cmdStr = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-block-size 100 --netbackup-filename /tmp/foo_schema > /tmp/foo_schema"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            restore_file_with_nbu(self.context, path="/tmp/foo_schema")
            cmd.assert_called_with("restoring metadata files to master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_no_path(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = 100
        cmdStr = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-block-size 100 --netbackup-filename /tmp/foo_schema > /tmp/foo_schema"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            restore_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("restoring metadata files to master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_both_args(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Cannot supply both a file type and a file path to restore_file_with_nbu'):
            restore_file_with_nbu(self.context, "schema", "/tmp/foo_schema")

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_neither_arg(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Cannot call restore_file_with_nbu with no type or path argument'):
            restore_file_with_nbu(self.context)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_block_size(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = 1024
        cmdStr = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-block-size 1024 --netbackup-filename /tmp/foo_schema > /tmp/foo_schema"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            restore_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("restoring metadata files to master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_keyword(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_keyword = "foo"
        cmdStr = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename /tmp/foo_schema > /tmp/foo_schema"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            restore_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("restoring metadata files to master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_restore_file_with_nbu_segment(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        cmdStr = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename /tmp/foo_schema > /tmp/foo_schema"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            restore_file_with_nbu(self.context, "schema", hostname="sdw")
            from gppylib.commands.base import REMOTE
            cmd.assert_called_with("restoring metadata files to segment", cmdStr, ctxt=REMOTE, remoteHost="sdw")

    @patch('gppylib.gparray.Segment.getSegmentHostName', return_value='sdw')
    def test_restore_config_files_with_nbu_default(self, mock1):
        with patch('gppylib.operations.restore.restore_file_with_nbu', side_effect=my_counter) as nbu_mock:
            global i
            i = 0
            self.context.netbackup_service_host = "mdw"
            self.context.netbackup_policy = "test_policy"
            self.context.netbackup_schedule = "test_schedule"

            restore_config_files_with_nbu(self.context)
            args, _ = nbu_mock.call_args_list[0]
            self.assertEqual(args[1], "master_config")
            for id, seg in enumerate(mock1.mock_segs):
                self.assertEqual(seg.get_active_primary.call_count, 1)
                self.assertEqual(seg.get_primary_dbid.call_count, 1)
                args, _ = nbu_mock.call_args_list[id]
            self.assertEqual(i, 3)

if __name__ == '__main__':
    unittest.main()

i=0
def my_counter(*args, **kwargs):
    global i
    i += 1
    return Mock()

