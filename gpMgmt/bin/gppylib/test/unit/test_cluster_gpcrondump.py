#!/usr/bin/env python

import os
import sys
import imp
gpcrondump_path = os.path.abspath('gpcrondump')
gpcrondump = imp.load_source('gpcrondump', gpcrondump_path)
import unittest2 as unittest
from datetime import datetime
from gppylib import gplog
from gpcrondump import GpCronDump
from gppylib.operations.utils import DEFAULT_NUM_WORKERS
from mock import patch, Mock, MagicMock, mock_open
from gppylib.commands.base import CommandResult
from gppylib.operations.dump import MailDumpEvent
from gppylib.operations.backup_utils import write_lines_to_file


logger = gplog.get_unittest_logger()

class GpcrondumpTestCase(unittest.TestCase):

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
            self.ddboost_show_config = False
            self.replicate = None
            self.max_streams = None
            self.netbackup_service_host = None
            self.netbackup_policy = None
            self.netbackup_schedule = None
            self.netbackup_block_size = None
            self.netbackup_keyword = None

    def setUp(self):
        self.options = self.Options()
        self.crondump = GpCronDump(self.options, None)
        self.context = self.crondump.context

    @patch('gpcrondump.GpCronDump.validate_dump_schema')
    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_1(self, mock, mock2):
        self.options.include_schema_file = '/tmp/foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '--schema-file option can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.GpCronDump.validate_dump_schema')
    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_2(self, mock, mock2):
        self.options.exclude_schema_file = '/tmp/foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '--exclude-schema-file option can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_3(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '-S option can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_4(self, mock):
        self.options.dump_schema = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '-s option can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_5(self, mock):
        self.options.dump_schema = 'foo'
        self.options.exclude_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-s can not be selected with --exclude-schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_6(self, mock):
        self.options.dump_schema = 'foo'
        self.options.include_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-s can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_7(self, mock):
        self.options.dump_schema = 'foo'
        self.options.exclude_dump_schema = 'foo'
        with self.assertRaisesRegexp(Exception, '-s can not be selected with -S option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_8(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.exclude_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-S can not be selected with --exclude-schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_9(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.include_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-S can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_10(self, mock):
        self.options.exclude_schema_file = 'foo'
        self.options.include_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--exclude-schema-file can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_11(self, mock):
        self.options.exclude_schema_file = 'foo'
        self.options.include_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with --exclude-schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_12(self, mock):
        self.options.exclude_schema_file = 'foo'
        self.options.exclude_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with --exclude-schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_13(self, mock):
        self.options.include_schema_file = 'foo'
        self.options.exclude_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_14(self, mock):
        self.options.include_schema_file = 'foo'
        self.options.include_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_15(self, mock):
        self.options.dump_schema = 'foo'
        self.options.include_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with -s option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_16(self, mock):
        self.options.dump_schema = 'foo'
        self.options.exclude_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with -s option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_17(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.include_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with -S option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_18(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.exclude_dump_tables_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '--table-file and --exclude-table-file can not be selected with -S option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_19(self, mock):
        self.options.exclude_schema_file = 'foo'
        self.options.exclude_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with --exclude-schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_20(self, mock):
        self.options.exclude_schema_file = 'foo'
        self.options.include_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with --exclude-schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_21(self, mock):
        self.options.include_schema_file = 'foo'
        self.options.exclude_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_22(self, mock):
        self.options.include_schema_file = 'foo'
        self.options.include_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with --schema-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_23(self, mock):
        self.options.dump_schema = 'foo'
        self.options.exclude_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with -s option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_24(self, mock):
        self.options.dump_schema = 'foo'
        self.options.include_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with -s option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_25(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.exclude_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with -S option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_26(self, mock):
        self.options.exclude_dump_schema = 'foo'
        self.options.include_dump_tables = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, '-t and -T can not be selected with -S option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_27(self, mock):
        self.options.dump_schema = ['information_schema']
        with self.assertRaisesRegexp(Exception, "can not specify catalog schema 'information_schema' using -s option"):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_28(self, mock):
        self.options.exclude_dump_schema = ['information_schema']
        with self.assertRaisesRegexp(Exception, "can not specify catalog schema 'information_schema' using -S option"):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_lines_from_file', return_value=['public', 'information_schema'])
    def test_schema_filter_29(self, mock, mock2):
        self.options.exclude_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, "can not exclude catalog schema 'information_schema' in schema file '/tmp/foo'"):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_lines_from_file', return_value=['public', 'information_schema'])
    def test_schema_filter_30(self, mock, mock2):
        self.options.include_schema_file = '/tmp/foo'
        with self.assertRaisesRegexp(Exception, "can not include catalog schema 'information_schema' in schema file '/tmp/foo'"):
            GpCronDump(self.options, None)

    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_31(self, mock, mock2):
        self.options.masterDataDirectory = '/tmp/foobar'
        gpcd = GpCronDump(self.options, None)
        with patch('__builtin__.open', mock_open(), create=True):
            gpcd.generate_schema_list_file()
            self.assertEquals(gpcd.context.schema_file, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_schema_filter_32(self, mock1):
        self.options.dump_schema = ['public']
        gpcd = GpCronDump(self.options, None)
        with patch('__builtin__.open', mock_open(), create=True):
            gpcd.generate_schema_list_file()
            self.assertTrue(gpcd.context.schema_file.startswith('/tmp/schema_list'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_lines_from_file', return_value=['public'])
    def test_schema_filter_33(self, mock1, mock2):
        self.options.include_schema_file = '/tmp/foo'
        gpcd = GpCronDump(self.options, None)
        with patch('__builtin__.open', mock_open(), create=True):
            gpcd.generate_schema_list_file()
            self.assertTrue(gpcd.context.schema_file.startswith('/tmp/schema_list'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_include_schema_list_from_exclude_schema', return_value=['public'])
    def test_schema_filter_34(self, mock1, mock2):
        self.options.exclude_schema_file = '/tmp/foo'
        write_lines_to_file('/tmp/foo', ['public'])
        gpcd = GpCronDump(self.options, None)
        with patch('__builtin__.open', mock_open(), create=True):
            gpcd.generate_schema_list_file()
            self.assertTrue(gpcd.context.schema_file.startswith('/tmp/schema_list'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_include_schema_list_from_exclude_schema', return_value=['public'])
    def test_schema_filter_35(self, mock1, mock2):
        self.options.exclude_dump_schema = 'public'
        gpcd = GpCronDump(self.options, None)
        with patch('__builtin__.open', mock_open(), create=True):
            gpcd.generate_schema_list_file()
            self.assertTrue(gpcd.context.schema_file.startswith('/tmp/schema_list'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_lines_from_file', return_value=['public'])
    @patch('gpcrondump.get_user_table_list_for_schema', return_value=['public', 'table1', 'public', 'table2'])
    def test_schema_filter_36(self, mock1, mock2, mock3):
        gpcd = GpCronDump(self.options, None)
        with patch('__builtin__.open', mock_open(), create=True):
            gpcd.generate_schema_list_file()
            inc = gpcd.generate_include_table_list_from_schema_file()
            self.assertTrue(inc.startswith('/tmp/include_dump_tables_file'))

    @patch('gpcrondump.validate_current_timestamp')
    def test_options1(self, mock):
        self.options.include_dump_tables = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, 'include table list can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options2(self, mock):
        self.options.exclude_dump_tables = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, 'exclude table list can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options3(self, mock):
        self.options.include_dump_tables_file = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, 'include table file can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options4(self, mock):
        self.options.exclude_dump_tables_file = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, 'exclude table file can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options10(self, mock):
        self.options.local_dump_prefix = 'foo'
        self.options.incremental = False
        self.options.list_filter_tables = True
        try:
            with self.assertRaisesRegexp(Exception, 'list filter tables option requires --prefix and --incremental'):
                cron = GpCronDump(self.options, None)
        finally:
            self.options.list_filter_tables = False

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_latest_full_dump_timestamp', return_value='20121225090000')
    def test_options11(self, mock, mock2):
        self.options.incremental = True
        cron = GpCronDump(self.options, None)
        self.assertEquals(cron.context.full_dump_timestamp, '20121225090000')

    @patch('gpcrondump.validate_current_timestamp')
    def test_options12(self, mock):
        self.options.incremental = True
        self.options.dump_databases = 'bkdb,fulldb'
        with self.assertRaisesRegexp(Exception, 'multi-database backup is not supported with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.get_latest_full_dump_timestamp', return_value='20120330090000')
    @patch('gpcrondump.validate_current_timestamp')
    def test_options13(self, mock, mock2):
        self.options.incremental = True
        self.options.dump_databases = ['bkdb']

        #If this is successful then it should not raise an exception
        GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options14(self, mock):
        self.options.dump_databases = 'bkdb'
        self.options.incremental = False

        #If this is successful then it should not raise an exception
        GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options15(self, mock):
        self.options.dump_databases = 'bkdb,fulldb'
        self.options.incremental = False

        #If this is successful then it should not raise an exception
        GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options16(self, mock):
        self.options.dump_schema = 'foo'
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '-s option can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options17(self, mock):
        self.options.clear_dumps = True
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '-c option can not be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options18(self, mock):
        self.options.dump_databases = []
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, 'Must supply -x <database name> with incremental option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options19(self, mock):
        self.options.ddboost = True
        self.options.replicate = False
        self.options.max_streams = 20
        with self.assertRaisesRegexp(Exception, '--max-streams must be specified along with --replicate'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options20(self, mock):
        self.options.ddboost = True
        self.options.replicate = True
        self.options.max_streams = None
        with self.assertRaisesRegexp(Exception, '--max-streams must be specified along with --replicate'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options21(self, mock):
        self.options.ddboost = True
        self.options.replicate = True
        self.options.max_streams = 0
        with self.assertRaisesRegexp(Exception, '--max-streams must be a number greater than zero'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options22(self, mock):
        self.options.ddboost = True
        self.options.replicate = True
        self.options.max_streams = "abc"
        with self.assertRaisesRegexp(Exception, '--max-streams must be a number greater than zero'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options23(self, mock):
        self.options.ddboost = False
        self.options.replicate = False
        self.options.max_streams = 20
        with self.assertRaisesRegexp(Exception, '--replicate and --max-streams cannot be used without --ddboost'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options24(self, mock1):
        self.options.list_backup_files = True
        self.options.timestamp_key = None
        with self.assertRaisesRegexp(Exception, 'Must supply -K option when listing backup files'):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options25(self, mock):
        self.options.dump_databases = 'bkdb,fulldb'
        self.options.timestamp_key = '20160101010101'
        with self.assertRaisesRegexp(Exception, 'multi-database backup is not supported with -K option'):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options27(self, mock):
        self.options.ddboost = True
        self.options.list_backup_files = True
        self.options.timestamp_key = '20160101010101'
        with self.assertRaisesRegexp(Exception, 'list backup files not supported with ddboost option'):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options28(self, mock):
        self.options.ddboost = True
        self.options.netbackup_service_host = "mdw"
        self.options.netbackup_policy = "test_policy"
        self.options.netbackup_schedule = "test_schedule"
        with self.assertRaisesRegexp(Exception, '--ddboost is not supported with NetBackup'):
            GpCronDump(self.options, None)

    def test_gpcrondump_options_h_H(self):
        testargs = ["","-h", "-H"]
        with patch.object(sys, 'argv', testargs):
            with self.assertRaisesRegexp(Exception, '-H option cannot be selected with -h option. '):
                self.crondump = GpCronDump(self.options, None)

    def test_options_ddboost_storage_unit_should_be_used_with_ddboost(self):
        """
        --ddboost-storage-unit option must come with --ddboost option
        """
        self.options.ddboost_storage_unit = "GPDB"
        with self.assertRaisesRegexp(Exception, 'Must specify --ddboost option together with the --ddboost-storage-unit'):
            GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_include_exclude_for_dump_database00(self, mock1, mock2):
        self.options.masterDataDirectory = '/tmp/foobar'
        gpcd = GpCronDump(self.options, None)
        dirtyfile = '/tmp/dirty'
        dbname = 'foo'
        (inc, exc) = gpcd.get_include_exclude_for_dump_database(dirtyfile, dbname)
        self.assertEquals(inc, None)
        self.assertEquals(exc, None)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.expand_partitions_and_populate_filter_file', return_value='/tmp/include_dump_tables_file')
    @patch('gpcrondump.get_lines_from_file', return_value=['public.t1', 'public.t2'])
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_include_exclude_for_dump_database01(self, mock1, mock2, mock3, mock4):
        self.options.masterDataDirectory = '/tmp/foobar'
        self.options.include_dump_tables_file = '/mydir/incfile'
        gpcd = GpCronDump(self.options, None)
        dirtyfile = '/tmp/dirty'
        dbname = 'foo'
        (inc, exc) = gpcd.get_include_exclude_for_dump_database(dirtyfile, dbname)
        self.assertTrue(inc.startswith('/tmp/include_dump_tables_file'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.expand_partitions_and_populate_filter_file', return_value='/tmp/include_dump_tables_file')
    @patch('gpcrondump.get_lines_from_file')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_include_exclude_for_dump_database02(self, mock1, mock2, mock3, mock4):
        self.options.masterDataDirectory = '/tmp/foobar'
        self.options.include_dump_tables = ['public.t1', 'public.t2', 'public.t3']
        gpcd = GpCronDump(self.options, None)
        dirtyfile = '/tmp/dirty'
        dbname = 'foo'
        (inc, exc) = gpcd.get_include_exclude_for_dump_database(dirtyfile, dbname)
        self.assertTrue(inc.startswith('/tmp/include_dump_tables_file'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_latest_full_dump_timestamp', return_value='20121225090000')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_include_exclude_for_dump_database03(self, mock1, mock2, mock3):
        self.options.masterDataDirectory = '/tmp/foobar'
        self.options.incremental = True
        gpcd = GpCronDump(self.options, None)
        dirtyfile = '/tmp/dirty'
        dbname = 'foo'
        (inc, exc) = gpcd.get_include_exclude_for_dump_database(dirtyfile, dbname)
        self.assertEquals(inc, '/tmp/dirty')
        self.assertEquals(exc, None)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.expand_partitions_and_populate_filter_file', return_value='/tmp/exclude_dump_tables_file')
    @patch('gpcrondump.get_lines_from_file', return_value=['public.t1', 'public.t2'])
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_include_exclude_for_dump_database04(self, mock1, mock2, mock3, mock4):
        self.options.masterDataDirectory = '/tmp/foobar'
        self.options.exclude_dump_tables_file = '/odir/exfile'
        gpcd = GpCronDump(self.options, None)
        dirtyfile = '/tmp/dirty'
        dbname = 'foo'
        (inc, exc) = gpcd.get_include_exclude_for_dump_database(dirtyfile, dbname)
        self.assertTrue(exc.startswith('/tmp/exclude_dump_tables_file'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.expand_partitions_and_populate_filter_file', return_value='/tmp/exclude_dump_tables_file')
    @patch('gpcrondump.get_lines_from_file')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_include_exclude_for_dump_database06(self, mock1, mock2, mock3, mock4):
        self.options.masterDataDirectory = '/tmp/foobar'
        self.options.exclude_dump_tables = ['public.t4', 'public.t5', 'public.t6']
        gpcd = GpCronDump(self.options, None)
        dirtyfile = '/tmp/dirty'
        dbname = 'foo'
        (inc, exc) = gpcd.get_include_exclude_for_dump_database(dirtyfile, dbname)
        self.assertTrue(exc.startswith('/tmp/exclude_dump_tables_file'))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.GpCronDump._get_table_names_from_partition_list', side_effect = [['public.aot1', 'public.aot2'], ['public.cot1', 'public.cot2']])
    def test_verify_tablenames_default(self, mock1, mock2):
        cron = GpCronDump(self.options, None)
        ao_partition_list = ['public, aot1, 2190', 'public, aot2, 3190']
        co_partition_list = ['public, cot1, 2190', 'public, cot2, 3190']
        heap_partition_list = ['public.heapt1', 'public.heapt2']
        cron._verify_tablenames(ao_partition_list, co_partition_list, heap_partition_list) #Should not raise an exception

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.GpCronDump._get_table_names_from_partition_list', side_effect = [['public.aot1:asd', 'public.aot2'], ['public.cot1', 'public.cot2:asd']])
    def test_verify_tablenames_bad_chars(self, mock1, mock2):
        cron = GpCronDump(self.options, None)
        ao_partition_list = ['public, aot1!asd, 2190', 'public, aot2, 3190']
        co_partition_list = ['public, cot1, 2190', 'public, cot2\nasd, 3190']
        heap_partition_list = ['public, heapt1, 2190', 'public, heapt2!asdasd , 3190']
        with self.assertRaisesRegexp(Exception, ''):
            cron._verify_tablenames(ao_partition_list, co_partition_list, heap_partition_list)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_inserts_with_incremental(self, mock):
        self.options.output_options = ['--inserts']
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '--inserts, --column-inserts, --oids cannot be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_oids_with_incremental(self, mock):
        self.options.output_options = ['--oids']
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '--inserts, --column-inserts, --oids cannot be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_column_inserts_with_incremental(self, mock):
        self.options.output_options = ['--column-inserts']
        self.options.incremental = True
        with self.assertRaisesRegexp(Exception, '--inserts, --column-inserts, --oids cannot be selected with incremental backup'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_get_table_names_from_partition_list_00(self, mock1):
        cron = GpCronDump(self.options, None)
        partition_list = ['public, aot1, 2190', 'public, aot2:aot, 3190']
        expected_output = ['public.aot1', 'public.aot2:aot']
        result = cron._get_table_names_from_partition_list(partition_list)
        self.assertEqual(result, expected_output)

    @patch('gpcrondump.validate_current_timestamp')
    def test_get_table_names_from_partition_list_01(self, mock1):
        cron = GpCronDump(self.options, None)
        partition_list = ['public, aot1, 2190', 'public, aot2,aot, 3190']
        with self.assertRaisesRegexp(Exception, 'Invalid partition entry "public, aot2,aot, 3190"'):
            cron._get_table_names_from_partition_list(partition_list)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_table_filter1(self, mock):
        self.options.include_dump_tables = 'foo'
        self.options.include_dump_tables_file = 'foo'
        with self.assertRaisesRegexp(Exception, '-t can not be selected with --table-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_table_filter2(self, mock):
        self.options.include_dump_tables = 'foo'
        self.options.exclude_dump_tables_file = 'foo'
        with self.assertRaisesRegexp(Exception, '-t can not be selected with --exclude-table-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_table_filter3(self, mock):
        self.options.exclude_dump_tables = 'foo'
        self.options.exclude_dump_tables_file = 'foo'
        with self.assertRaisesRegexp(Exception, '-T can not be selected with --exclude-table-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_table_filter4(self, mock):
        self.options.exclude_dump_tables = 'foo'
        self.options.include_dump_tables_file = 'foo'
        with self.assertRaisesRegexp(Exception, '-T can not be selected with --table-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_table_filter5(self, mock):
        self.options.include_dump_tables = 'foo'
        self.options.exclude_dump_tables = 'foo'
        with self.assertRaisesRegexp(Exception, '-t can not be selected with -T option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    def test_options_table_filter6(self, mock):
        self.options.include_dump_tables_file = 'foo'
        self.options.exclude_dump_tables_file = 'foo'
        with self.assertRaisesRegexp(Exception, '--table-file can not be selected with --exclude-table-file option'):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_files_file_list1(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.masterDataDirectory = '/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        files_file_list = gpcd._get_files_file_list(master)
        expected_files_list = ['foo1:%s/db_dumps/20130101/gp_cdatabase_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101_ao_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101_co_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101_last_operation' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101.rpt' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_status_1_1_20130101010101' % self.options.masterDataDirectory]
        self.assertEqual(files_file_list, expected_files_list)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_files_file_list2(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.masterDataDirectory = '/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo2'
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        files_file_list = gpcd._get_files_file_list(master)
        expected_files_list = ['foo2:%s/db_dumps/20130101/gp_cdatabase_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo2:%s/db_dumps/20130101/gp_dump_20130101010101_ao_state_file' % self.options.masterDataDirectory,
                               'foo2:%s/db_dumps/20130101/gp_dump_20130101010101_co_state_file' % self.options.masterDataDirectory,
                               'foo2:%s/db_dumps/20130101/gp_dump_20130101010101_last_operation' % self.options.masterDataDirectory,
                               'foo2:%s/db_dumps/20130101/gp_dump_20130101010101.rpt' % self.options.masterDataDirectory,
                               'foo2:%s/db_dumps/20130101/gp_dump_status_1_1_20130101010101' % self.options.masterDataDirectory]
        self.assertEqual(files_file_list, expected_files_list)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_latest_full_dump_timestamp', return_value='20130101000000')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_files_file_list3(self, mock1, mock2, mock3):
        self.options.timestamp_key = '20130101010101'
        self.options.incremental = True
        self.options.masterDataDirectory = '/data/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        gpcd.context.timestamp = '20130101010101'

        dump_dir = gpcd.context.get_backup_dir()
        files_file_list = gpcd._get_files_file_list(master)
        expected_files_list = ['foo1:%s/db_dumps/20130101/gp_cdatabase_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101_ao_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101_co_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101_last_operation' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101010101.rpt' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_status_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_20130101000000_increments' % self.options.masterDataDirectory]
        self.assertEqual(sorted(files_file_list), sorted(expected_files_list))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.get_latest_full_dump_timestamp', return_value='20130101000000')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_files_file_list_with_filter(self, mock1, mock2, mock3):
        self.options.timestamp_key = '20130101010101'
        self.options.local_dump_prefix = 'metro'
        self.options.include_dump_tables_file = 'bar'
        self.options.masterDataDirectory = '/data/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        files_file_list = gpcd._get_files_file_list(master)
        expected_files_list = ['foo1:%s/db_dumps/20130101/metro_gp_cdatabase_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_ao_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_co_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_last_operation' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101.rpt' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_status_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_filter' % self.options.masterDataDirectory]
        self.assertEqual(sorted(files_file_list), sorted(expected_files_list))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gpcrondump.get_latest_full_dump_timestamp', return_value='20130101000000')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_files_file_list_with_prefix(self, mock1, mock2, mock3):
        self.options.timestamp_key = '20130101010101'
        self.options.incremental = True
        self.options.local_dump_prefix = 'metro'
        self.options.masterDataDirectory = '/data/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        gpcd.context.timestamp = '20130101010101'

        dump_dir = gpcd.context.get_backup_dir()
        files_file_list = gpcd._get_files_file_list(master)
        expected_files_list = ['foo1:%s/db_dumps/20130101/metro_gp_cdatabase_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_ao_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_co_state_file' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101_last_operation' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101010101.rpt' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_status_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/metro_gp_dump_20130101000000_increments' % self.options.masterDataDirectory]

        self.assertEqual(sorted(files_file_list), sorted(expected_files_list))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_pipes_file_list1(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.masterDataDirectory = '/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo2'
        mock_segs = []
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        pipes_file_list = gpcd._get_pipes_file_list(master, mock_segs)
        expected_files_list = ['foo2:%s/db_dumps/20130101/gp_dump_1_1_20130101010101.gz' % self.options.masterDataDirectory,
                               'foo2:%s/db_dumps/20130101/gp_dump_1_1_20130101010101_post_data.gz' % self.options.masterDataDirectory]
        self.assertEqual(pipes_file_list, expected_files_list)

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_pipes_file_list2(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.masterDataDirectory = '/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.getSegmentDataDirectory.return_value = '/bar'
            seg.getSegmentHostName.return_value = 'foo1'
            seg.getSegmentDbId.return_value = id + 2
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        pipes_file_list = gpcd._get_pipes_file_list(master, mock_segs)
        expected_files_list = ['foo1:%s/db_dumps/20130101/gp_dump_1_1_20130101010101.gz' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_1_1_20130101010101_post_data.gz' % self.options.masterDataDirectory,
                               'foo1:/bar/db_dumps/20130101/gp_dump_0_2_20130101010101.gz',
                               'foo1:/bar/db_dumps/20130101/gp_dump_0_3_20130101010101.gz']
        self.assertEqual(sorted(pipes_file_list), sorted(expected_files_list))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_pipes_file_list3(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.dump_global = True
        self.options.masterDataDirectory = '/foo'
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.getSegmentDataDirectory.return_value = '/bar'
            seg.getSegmentHostName.return_value = 'foo1'
            seg.getSegmentDbId.return_value = id + 2
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        pipes_file_list = gpcd._get_pipes_file_list(master, mock_segs)
        expected_files_list = ['foo1:%s/db_dumps/20130101/gp_dump_1_1_20130101010101.gz' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_1_1_20130101010101_post_data.gz' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_global_1_1_20130101010101' % self.options.masterDataDirectory,
                               'foo1:/bar/db_dumps/20130101/gp_dump_0_2_20130101010101.gz',
                               'foo1:/bar/db_dumps/20130101/gp_dump_0_3_20130101010101.gz']
        self.assertEqual(sorted(pipes_file_list), sorted(expected_files_list))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_get_pipes_file_list4(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.masterDataDirectory = '/foo'
        self.options.dump_config = True
        gpcd = GpCronDump(self.options, None)
        master = Mock()
        master.getSegmentHostName.return_value = 'foo1'
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.getSegmentDataDirectory.return_value = '/bar'
            seg.getSegmentHostName.return_value = 'foo1'
            seg.getSegmentDbId.return_value = id + 2
        gpcd.context.timestamp = '20130101010101'
        dump_dir = gpcd.context.get_backup_dir()
        pipes_file_list = gpcd._get_pipes_file_list(master, mock_segs)
        expected_files_list = ['foo1:%s/db_dumps/20130101/gp_dump_1_1_20130101010101.gz' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_dump_1_1_20130101010101_post_data.gz' % self.options.masterDataDirectory,
                               'foo1:%s/db_dumps/20130101/gp_master_config_files_20130101010101.tar' % self.options.masterDataDirectory,
                               'foo1:/bar/db_dumps/20130101/gp_segment_config_files_0_2_20130101010101.tar',
                               'foo1:/bar/db_dumps/20130101/gp_segment_config_files_0_3_20130101010101.tar',
                               'foo1:/bar/db_dumps/20130101/gp_dump_0_2_20130101010101.gz',
                               'foo1:/bar/db_dumps/20130101/gp_dump_0_3_20130101010101.gz']
        self.assertEqual(sorted(pipes_file_list), sorted(expected_files_list))

    @patch('gpcrondump.validate_current_timestamp')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_gpcrondump_init0(self, mock1, mock2):
        self.options.timestamp_key = None
        self.options.local_dump_prefix = 'foo'
        self.options.ddboost = False
        self.options.ddboost_verify = False
        self.options.ddboost_config_remove = False
        self.options.ddboost_user = False
        self.options.ddboost_host = False
        self.options.max_streams = None
        self.options.list_backup_files = False
        gpcd = GpCronDump(self.options, None)
        self.assertEqual(gpcd.context.dump_prefix, 'foo_')

    @patch('gpcrondump.os.path.isfile', return_value=True)
    @patch('gpcrondump.os.path.getsize', return_value=111)
    @patch('gpcrondump.yaml.load', return_value={'EMAIL_DETAILS': [{'FROM': 'RRP_MPE2_DCA_1', 'DBNAME': 'testdb100', 'SUBJECT': "backup completed for Database 'testdb100'"}]})
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_validate_parse_email_File00(self, mock1, mock2, mock3, mock4):
        self.options.include_email_file = "/tmp/abc.yaml"
        m = MagicMock()
        with patch('__builtin__.open', m, create=True):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.os.path.isfile', return_value=False)
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_validate_parse_email_File01(self, mock1, mock2):
        self.options.include_email_file = "/tmp/abc.yaml"
        with self.assertRaisesRegexp(Exception, "\'%s\' file does not exist." % self.options.include_email_file):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.os.path.isfile', return_value=True)
    @patch('gpcrondump.os.path.getsize', return_value=111)
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_validate_parse_email_File02(self, mock1, mock2, mock3):
        self.options.include_email_file = "/tmp/abc"
        with self.assertRaisesRegexp(Exception, "'%s' is not '.yaml' file. File containing email details should be '.yaml' file." %  self.options.include_email_file):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.os.path.isfile', return_value=True)
    @patch('gpcrondump.os.path.getsize', return_value=0)
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_validate_parse_email_File03(self, mock1, mock2, mock3):
        self.options.include_email_file = "/tmp/abc.yaml"
        with self.assertRaisesRegexp(Exception, "'%s' file is empty." % self.options.include_email_file):
            cron = GpCronDump(self.options, None)

    @patch('gpcrondump.os.path.isfile', return_value=True)
    @patch('gpcrondump.os.path.getsize', return_value=111)
    @patch('gpcrondump.yaml.load', return_value={'EMAIL_DETAILS': [{'FROM': 'RRP_MPE2_DCA_1', 'NAME': 'testdb100', 'SUBJECT': "backup completed for Database 'testdb100'"}]})
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_validate_parse_email_File04(self, mock1, mock2, mock3, mock4):
        self.options.include_email_file = "/tmp/abc.yaml"
        m = MagicMock()
        with self.assertRaisesRegexp(Exception, "\'%s\' file is not formatted properly." % self.options.include_email_file):
            with patch('__builtin__.open', m, create=True):
                cron = GpCronDump(self.options, None)

    @patch('gpcrondump.os.path.isfile', return_value=True)
    @patch('gpcrondump.os.path.getsize', return_value=111)
    @patch('gpcrondump.yaml.load', return_value={'EMAIL_DETAILS': [{'FROM': 'RRP_MPE2_DCA_1', 'DBNAME': None, 'SUBJECT': "backup completed for Database 'testdb100'"}]})
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_validate_parse_email_File05(self, mock1, mock2, mock3, mock4):
        self.options.include_email_file = "/tmp/abc.yaml"
        m = MagicMock()
        with self.assertRaisesRegexp(Exception, "\'%s\' file is not formatted properly." % self.options.include_email_file):
            with patch('__builtin__.open', m, create=True):
                cron = GpCronDump(self.options, None)

    @patch('gpcrondump.MailDumpEvent')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_send_email00(self, mock1, MailDumpEvent):
        dump_database = 'testdb1'
        current_exit_status = 0
        time_start = '12:07:09'
        time_end = '12:08:18'
        cron = GpCronDump(self.options, None)
        cron._send_email(dump_database, current_exit_status, time_start, time_end)

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_show_ddboost_config_local_DDServer(self, mock_get_pgport, mock_result, mock_run):
        mock_result.return_value = CommandResult(0, '''20160510:15:45:01|ddboost-[DEBUG]:-Libraries were loaded successfully
20160510:15:45:01|ddboost-[INFO]:-opening LB on /home/gpadmin/DDBOOST_CONFIG
Data Domain Hostname:qadd01
Data Domain Boost Username:metro
Default Backup Directory:MY_MFR
Data Domain default log level:WARNING
Data Domain Storage Unit:GPDB''', '', False, True)
        self.options.ddboost_show_config = True
        gpcd = GpCronDump(self.options, None)
        logger.info = Mock()
        gpcd.show_ddboost_config()
        logger.info.assert_any_call('Data Domain Hostname:qadd01')
        logger.info.assert_any_call('Data Domain Boost Username:metro')
        logger.info.assert_any_call('Default Backup Directory:MY_MFR')
        logger.info.assert_any_call('Data Domain default log level:WARNING')
        logger.info.assert_any_call('Data Domain Storage Unit:GPDB')


    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results')
    @patch('gppylib.operations.backup_utils.Context.get_master_port')
    def test_show_ddboost_config_remote_DDServer(self, mock_get_pgport, mock_result, mock_run):
        mock_result.return_value = CommandResult(0, '''20160511:10:28:06|ddboost-[DEBUG]:-Libraries were loaded successfully
20160511:10:28:06|ddboost-[INFO]:-opening LB on /home/gpadmin/DDBOOST_MFR_CONFIG
Data Domain Hostname:dd1
Data Domain Boost Username:gpadmin
Data Domain default log level:WARNING
Data Domain default log size:50''', '', False, True)
        self.options.ddboost_show_config = True
        gpcd = GpCronDump(self.options, None)
        logger.info = Mock()
        gpcd.show_ddboost_config()
        logger.info.assert_any_call('Data Domain Hostname:dd1')
        logger.info.assert_any_call('Data Domain Boost Username:gpadmin')
        logger.info.assert_any_call('Data Domain default log level:WARNING')
        logger.info.assert_any_call('Data Domain default log size:50')

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
