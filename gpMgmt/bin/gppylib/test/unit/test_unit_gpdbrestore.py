#!/usr/bin/env python

import os
import sys
import imp
gpdbrestore_path = os.path.abspath('gpdbrestore')
gpdbrestore = imp.load_source('gpdbrestore', gpdbrestore_path)
from datetime import datetime
from gpdbrestore import GpdbRestore
from gppylib import gplog
from gppylib.commands.base import CommandResult
from gppylib.mainUtils import ExceptionNoStackTraceNeeded, ProgramArgumentValidationException
from gppylib.operations.backup_utils import write_lines_to_file
from gppylib.operations.dump import MailDumpEvent
from gppylib.operations.utils import DEFAULT_NUM_WORKERS
from mock import patch, Mock, MagicMock, mock_open
import unittest
from . import setup_fake_gparray


logger = gplog.get_unittest_logger()

@patch('gppylib.gparray.GpArray.initFromCatalog', return_value=setup_fake_gparray())
class GpdbrestoreTestCase(unittest.TestCase):

    class Options:
        def __init__(self):
            self.masterDataDirectory = ""
            self.interactive = False
            self.clear_dumps_only = False
            self.post_script = None
            self.dump_config = False
            self.history = False
            self.pre_vacuum = False
            self.post_vacuum = False
            self.rollback = False
            self.compress = True
            self.free_space_percent = None
            self.clear_dumps = False
            self.cleanup_date = None
            self.cleanup_total = None
            self.dump_schema = False
            self.dump_databases = ['testdb']
            self.bypass_disk_check = True
            self.backup_set = None
            self.dump_global = False
            self.clear_catalog_dumps = False
            self.batch_default = DEFAULT_NUM_WORKERS
            self.include_dump_tables = None
            self.exclude_dump_tables = None
            self.include_dump_tables_file = None
            self.exclude_dump_tables_file = None
            self.backup_dir = None
            self.encoding = None
            self.output_options = None
            self.report_dir = None
            self.timestamp_key = None
            self.list_backup_files = None
            self.quiet = False
            self.verbose = False
            self.local_dump_prefix = ''
            self.list_filter_tables = None
            self.include_email_file = None
            self.email_details = None
            self.include_schema_file = None
            self.exclude_schema_file = None
            self.exclude_dump_schema = None
            self.dump_stats = None

            self.search_for_dbname = None
            self.timestamp = "20160101010101"
            self.db_date_dir = None
            self.db_host_path = None
            self.list_tables = False
            self.list_backup = False
            self.truncate = False
            self.table_file = None
            self.master_datadir=self.backup_dir="/data/master"

            ## Enterprise init
            self.incremental = False
            self.ddboost = False
            self.ddboost_storage_unit = None
            self.ddboost_hosts = None
            self.ddboost_user = None
            self.ddboost_config_remove = False
            self.ddboost_verify = False
            self.ddboost_remote = None
            self.ddboost_ping = None
            self.ddboost_backupdir = None
            self.ddboost_storage_unit = None
            self.replicate = None
            self.max_streams = None
            self.netbackup_service_host = None
            self.netbackup_policy = None
            self.netbackup_schedule = None
            self.netbackup_block_size = None
            self.netbackup_keyword = None

    @patch('gppylib.gparray.GpArray.initFromCatalog', return_value=setup_fake_gparray())
    def setUp(self, mock1):
        self.options = self.Options()
        self.gpdbrestore = GpdbRestore(self.options, None)
        self.context = self.gpdbrestore.context

    @patch('gppylib.commands.unix.Ping.local')
    @patch('gppylib.commands.unix.Scp.run')
    @patch('gppylib.operations.backup_utils.Context.get_compress_and_dbname_from_report_file', return_value=('False', 'gptest'))
    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.unix.RawRemoteOperation.execute', return_value=True)
    @patch('gppylib.operations.unix.ListFilesByPattern.run', return_value=['gp_dump_20160101010101.rpt'])
    @patch('gppylib.operations.unix.ListRemoteFilesByPattern.run', return_value=['gp_dump_20160101010101.rpt'])
    @patch('os.remove')
    def test_validate_db_host_path_default(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7, mock8, mock9):
        self.gpdbrestore.context.db_host_path = ['hostname', '/tmp/db_dumps']
        self.gpdbrestore._validate_db_host_path()
        self.assertEquals(self.context.backup_dir, '/tmp')

    def test_validate_db_host_path_invalid_argument_one_item(self, mock1):
        self.gpdbrestore.context.db_host_path = ['hostname']
        with self.assertRaises(ProgramArgumentValidationException):
            self.gpdbrestore._validate_db_host_path()

    def test_validate_db_host_path_invalid_argument_three_items(self, mock1):
        self.gpdbrestore.context.db_host_path = ['hostname', '/tmp/db_dumps', '~/db_dumps']
        with self.assertRaises(ProgramArgumentValidationException):
            self.gpdbrestore._validate_db_host_path()

    def test_validate_db_host_path_no_dump_dir_in_path(self, mock1):
        self.context.db_host_path = ['hostname', '/tmp/path']
        with self.assertRaisesRegexp(Exception, 'does not contain expected "db_dumps" directory'):
            self.gpdbrestore._validate_db_host_path()

    @patch('gppylib.operations.backup_utils.Context.get_compress_and_dbname_from_report_file',
            side_effect=[(False, "gptest1"), (False, "gptest2")])
    @patch('gppylib.operations.backup_utils.Context.get_report_files_and_paths',
            return_value=[('path','gp_dump_20160101010101.rpt'), ('path', 'gp_dump_20160101010102.rpt')])
    def test_get_dump_timestamps_for_database_one_matching_file(self, mock1, mock2, mock3):
        dbname = 'gptest2'
        expected_timestamps = [20160101010102]
        actual_timestamps = self.gpdbrestore._get_dump_timestamps_for_database(dbname, "/tmp")
        self.assertEquals(expected_timestamps, actual_timestamps)

    @patch('gppylib.operations.backup_utils.Context.get_compress_and_dbname_from_report_file',
            side_effect=[(False, "gptest2"), (False, "gptest1"), (False, "gptest2")])
    @patch('gppylib.operations.backup_utils.Context.get_report_files_and_paths',
            return_value=[('path', 'gp_dump_20160101010101.rpt'),
                          ('path', 'gp_dump_20160101010102.rpt'),
                          ('path', 'gp_dump_20160101010103.rpt')])
    def test_get_dump_timestamps_for_database_two_matching_files(self, mock1, mock2, mock3):
        dbname = 'gptest2'
        expected_timestamps = [20160101010101, 20160101010103]
        actual_timestamps = self.gpdbrestore._get_dump_timestamps_for_database(dbname, "/tmp")
        self.assertEquals(expected_timestamps, actual_timestamps)

    @patch('gppylib.operations.backup_utils.Context.get_compress_and_dbname_from_report_file',
            side_effect=[(False, "gptest1"), (False, "gptest2")])
    @patch('gppylib.operations.backup_utils.Context.get_report_files_and_paths',
            return_value=[('path','gp_dump_20160101010101.rpt'), ('path', 'gp_dump_20160101010102.rpt')])
    def test_get_dump_timestamps_for_database_no_report_files_for_dbname(self, mock1, mock2, mock3):
        dbname = 'gptest'
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, 'No report files located with database gptest'):
            self.gpdbrestore._get_dump_timestamps_for_database(dbname, "/tmp")

    @patch('gppylib.operations.unix.ListFilesByPattern.run', return_value=[])
    def test_get_timestamp_for_validation_no_timestamps(self, mock1, mock2):
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, "Could not locate Master database dump files in"):
            self.gpdbrestore._get_timestamp_for_validation("/tmp")

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.unix.ListFilesByPattern.run', return_value=['gp_dump_20160101010101.rpt'])
    def test_get_timestamp_for_validation_one_timestamp(self, mock1, mock2, mock3):
        self.gpdbrestore._get_timestamp_for_validation("/tmp")
        self.assertEquals(self.context.timestamp, '20160101010101')

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.unix.ListFilesByPattern.run', return_value=['gp_dump_20160101010101.rpt', 'gp_dump_20160101010102.rpt'])
    @patch('gpdbrestore.GpdbRestore._select_multi_file')
    def test_get_timestamp_for_validation_two_timestamps(self, mock1, mock2, mock3, mock4):
        self.gpdbrestore._get_timestamp_for_validation("/tmp")
        timestamp_list = [('20160101','010101'), ('20160101','010102')]
        mock1.assert_called_once_with(timestamp_list)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.unix.ListFilesByPattern.run', return_value=['gp_dump_20160101010102.rpt', 'gp_dump_20160101010103.rpt', 'gp_dump_20160101010101.rpt'])
    @patch('gpdbrestore.GpdbRestore._select_multi_file')
    def test_get_timestamp_for_validation_timestamps_out_of_order(self, mock1, mock2, mock3, mock4):
        self.gpdbrestore._get_timestamp_for_validation("/tmp")
        timestamp_list = [('20160101','010101'), ('20160101','010102'), ('20160101','010103')]
        mock1.assert_called_once_with(timestamp_list)

    @patch('__builtin__.raw_input', return_value='0')
    def test_select_multi_file_default(self, mock1, mock2):
        timestamp_list = [('20160101','010101'), ('20160101','010102')]
        result = self.gpdbrestore._select_multi_file(timestamp_list)
        self.assertEquals(result, '20160101010101')

    @patch('__builtin__.raw_input', return_value='')
    def test_select_multi_file_no_input(self, mock1, mock2):
        timestamp_list = [('20160101','010101'), ('20160101','010102')]
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, "Invalid or null input"):
            self.gpdbrestore._select_multi_file(timestamp_list)

    @patch('__builtin__.raw_input', return_value='foo')
    def test_select_multi_file_non_digit_input(self, mock1, mock2):
        timestamp_list = [('20160101','010101'), ('20160101','010102')]
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, "Invalid or null input"):
            self.gpdbrestore._select_multi_file(timestamp_list)

    @patch('__builtin__.raw_input', return_value='-1')
    def test_select_multi_file_input_num_too_small(self, mock1, mock2):
        timestamp_list = [('20160101','010101'), ('20160101','010102')]
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, "Invalid or null input"):
            self.gpdbrestore._select_multi_file(timestamp_list)

    @patch('__builtin__.raw_input', return_value='3')
    def test_select_multi_file_input_num_too_large(self, mock1, mock2):
        timestamp_list = [('20160101','010101'), ('20160101','010102')]
        with self.assertRaisesRegexp(ExceptionNoStackTraceNeeded, "Invalid or null input"):
            self.gpdbrestore._select_multi_file(timestamp_list)

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
