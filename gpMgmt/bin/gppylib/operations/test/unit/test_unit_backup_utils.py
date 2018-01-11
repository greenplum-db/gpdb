#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2016. All Rights Reserved.
#

import os
import shutil
import unittest
from gppylib.commands.base import CommandResult
from gppylib.gparray import GpArray, Segment
from gppylib.operations.backup_utils import *
from . import setup_fake_gparray

from mock import patch, Mock

from test.unit.gp_unittest import GpTestCase

class BackupUtilsTestCase(GpTestCase):

    def setUp(self):
        with patch('gppylib.gparray.GpArray.initFromCatalog', return_value=setup_fake_gparray()):
            self.context = Context()
            self.context.master_datadir = '/data/master'
            self.context.backup_dir = None
            self.context.timestamp = '20160101010101'
            self.netbackup_filepath = "/tmp/db_dumps/foo"

    def test_generate_filename_dump_master_old_format(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_1_1_20160101010101.gz'
        output = self.context.generate_filename("dump", use_old_format=True)
        self.assertEquals(output, expected_output)

    def test_generate_filename_dump_segment_old_format(self):
        dbid = 3
        expected_output = '/data/primary1/db_dumps/20160101/gp_dump_0_3_20160101010101.gz'
        output = self.context.generate_filename("dump", dbid=dbid, use_old_format=True)
        self.assertEquals(output, expected_output)

    def test_generate_filename_dump_master_new_format(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_-1_1_20160101010101.gz'
        output = self.context.generate_filename("dump")
        self.assertEquals(output, expected_output)

    def test_generate_filename_dump_segment_new_format(self):
        dbid = 3
        expected_output = '/data/primary1/db_dumps/20160101/gp_dump_1_3_20160101010101.gz'
        output = self.context.generate_filename("dump", dbid=dbid)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_master_new_format_no_standby(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_-1_(1)_20160101010101.gz'
        output = self.context.generate_filename("dump", content=-1)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_segment_new_format_no_mirror(self):
        content = 1
        expected_output = '/data/master/db_dumps/20160101/gp_dump_1_(3)_20160101010101.gz'
        del self.context.content_map[5]
        output = self.context.generate_filename("dump", content=content)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_master_new_format_with_standby(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_-1_(1|10)_20160101010101.gz'
        self.context.content_map[10] = -1
        output = self.context.generate_filename("dump", content=-1)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_segment_new_format_with_mirror(self):
        content = 1
        expected_output = '/data/master/db_dumps/20160101/gp_dump_1_(3|5)_20160101010101.gz'
        output = self.context.generate_filename("dump", content=content)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_master_old_format_no_standby(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_1_(1)_20160101010101.gz'
        self.context.use_old_filename_format = True
        output = self.context.generate_filename("dump", content=-1)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_segment_old_format_no_mirror(self):
        content = 1
        expected_output = '/data/master/db_dumps/20160101/gp_dump_0_(3)_20160101010101.gz'
        self.context.use_old_filename_format = True
        del self.context.content_map[5]
        output = self.context.generate_filename("dump", content=content)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_master_old_format_with_standby(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_1_(1|10)_20160101010101.gz'
        self.context.use_old_filename_format = True
        self.context.content_map[10] = -1
        output = self.context.generate_filename("dump", content=-1)
        self.assertEquals(output, expected_output)

    def test_generate_filename_content_segment_old_format_with_mirror(self):
        content = 1
        expected_output = '/data/master/db_dumps/20160101/gp_dump_0_(3|5)_20160101010101.gz'
        self.context.use_old_filename_format = True
        output = self.context.generate_filename("dump", content=content)
        self.assertEquals(output, expected_output)

    def test_generate_filename_different_backup_dir(self):
        self.context.backup_dir = '/data/masterdomain'
        expected_output = '/data/masterdomain/db_dumps/20160101/gp_dump_20160101010101_schema'
        output = self.context.generate_filename("schema")
        self.assertEquals(output, expected_output)

    def test_generate_filename_no_mdd(self):
        self.context.master_datadir = None
        self.context.backup_dir = '/data/masterdomain'
        expected_output = '/data/masterdomain/db_dumps/20160101/gp_dump_20160101010101_schema'
        output = self.context.generate_filename("schema")
        self.assertEquals(output, expected_output)

    def test_generate_filename_no_mdd_or_backup_dir(self):
        self.context.master_datadir = None
        with self.assertRaisesRegexp(Exception, 'Cannot locate backup directory with existing parameters'):
            self.context.generate_filename("schema")

    def test_generate_filename_no_timestamp(self):
        self.context.timestamp = None
        with self.assertRaisesRegexp(Exception, 'Cannot locate backup directory without timestamp'):
            self.context.generate_filename("schema")

    def test_generate_filename_bad_timestamp(self):
        self.context.timestamp = 'xx160101010101'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            self.context.generate_filename("schema")

    def test_generate_filename_short_timestamp(self):
        self.context.timestamp = '2016'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            self.context.generate_filename("schema")

    def test_validate_timestamp_default(self):
        ts = "20160101010101"
        result = validate_timestamp(ts)
        self.assertTrue(result)

    def test_validate_timestamp_too_short(self):
        ts = "2016010101010"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test_validate_timestamp_too_long(self):
        ts = "201601010101010"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test_validate_timestamp_zero(self):
        ts = "00000000000000"
        result = validate_timestamp(ts)
        self.assertTrue(result)

    def test_validate_timestamp_hex(self):
        ts = "0a000000000000"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test_validate_timestamp_leading_space(self):
        ts = " 00000000000000"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test_validate_timestamp_trailing_space(self):
        ts = "00000000000000 "
        result = validate_timestamp(ts)
        self.assertFalse(result);

    def test_validate_timestamp_none(self):
        ts = None
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test_generate_filename_with_timestamp(self):
        ts = '20150101010101'
        expected_output = '/data/master/db_dumps/20150101/gp_dump_20150101010101_increments'
        output = self.context.generate_filename("increments", timestamp=ts)
        self.assertEquals(output, expected_output)

    def test_generate_filename_with_ddboost(self):
        self.context.ddboost = True
        self.context.backup_dir = "/tmp"
        expected_output = '/data/master/db_dumps/20160101/gp_dump_20160101010101_increments'
        output = self.context.generate_filename("increments")
        self.assertEquals(output, expected_output)

    @patch('os.path.exists', side_effect=[True])
    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file',
         return_value=['BackupFile /data/master/db_dumps/20160101/gp_dump_1_1_20160101010101.gz: Succeeded'])
    def test_is_timestamp_in_old_format_old(self, mock1, mock2, mock3):
        self.assertTrue(self.context.is_timestamp_in_old_format())

    @patch('os.path.exists', side_effect=[True])
    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file',
         return_value=['BackupFile /data/master/db_dumps/20160101/gp_dump_-1_1_20160101010101.gz: Succeeded'])
    def test_is_timestamp_in_old_format_new(self, mock1, mock2, mock3):
        self.assertFalse(self.context.is_timestamp_in_old_format())

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file',
         return_value=['BackupFile /data/master/db_dumps/20160101/gp_dump_1_1_20160101010101.gz: Succeeded'])
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    def test_is_timestamp_in_old_format_old_nbu(self, mock1, mock2, mock3):
        self.context.netbackup_service_host = "netbackup-service"
        self.assertTrue(self.context.is_timestamp_in_old_format())

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file',
         return_value=['BackupFile /data/master/db_dumps/20160101/gp_dump_-1_1_20160101010101.gz: Succeeded'])
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    def test_is_timestamp_in_old_format_new_nbu(self, mock1, mock2, mock3):
        self.context.netbackup_service_host = "netbackup-service"
        self.assertFalse(self.context.is_timestamp_in_old_format())

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=[])
    def test_is_timestamp_in_old_format_no_dump_dirs(self, mock1):
        with self.assertRaisesRegexp(Exception, "Unable to locate report file for timestamp"):
			self.context.is_timestamp_in_old_format()

    @patch('os.path.exists', side_effect=[False])
    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    def test_is_timestamp_in_old_format_no_report_file(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, "Unable to locate report file for timestamp"):
			self.context.is_timestamp_in_old_format()

    @patch('os.path.exists', side_effect=[True])
    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[''])
    def test_is_timestamp_in_old_format_empty_report_file(self, mock1, mock2, mock3):
        self.assertFalse(self.context.is_timestamp_in_old_format())

    @patch('os.path.exists', side_effect=[True])
    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['/tmp/db_dumps/20160101'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file',
         return_value=['BackupFile /backup/DCA/20160101/gp_dump_1_1_20160101010101.gz: Succeeded'])
    def test_is_timestamp_in_old_format_wrong_path(self, mock1, mock2, mock3):
        self.assertTrue(self.context.is_timestamp_in_old_format())

    @patch('glob.glob', return_value=['/data/master/db_dumps/20160101/gp_dump_1_1_20160101010101.gz'])
    def test_get_filename_for_content_old_format_master_exists(self, mock1):
        self.context.use_old_filename_format=True
        filename = get_filename_for_content(self.context, "metadata", -1)
        self.assertEquals(filename, '/data/master/db_dumps/20160101/gp_dump_1_1_20160101010101.gz')

    @patch('glob.glob', return_value=['/data/master/db_dumps/20160101/gp_dump_-1_1_20160101010101.gz'])
    def test_get_filename_for_content_new_format_master_exists(self, mock1):
        self.context.use_old_filename_format=False
        filename = get_filename_for_content(self.context, "metadata", -1)
        self.assertEquals(filename, '/data/master/db_dumps/20160101/gp_dump_-1_1_20160101010101.gz')

    @patch('glob.glob', return_value=[])
    def test_get_filename_for_content_master_doesnt_exist(self, mock1):
        filename = get_filename_for_content(self.context, "metadata", -1)
        self.assertEquals(filename, None)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_get_filename_for_content_segment_exists(self, mock1):
        cmd_mock = Mock()
        cmd_mock.rc = 0
        cmd_mock.stdout = '/data/master/db_dumps/20160101/gp_dump_1_3_20160101010101.gz'
        cmd = Mock()
        with patch('gppylib.operations.dump.Command.get_results', return_value=cmd_mock):
            filename = get_filename_for_content(self.context, "metadata", 3, '/data/master', 'remoteHost')
            self.assertEquals(filename, '/data/master/db_dumps/20160101/gp_dump_1_3_20160101010101.gz')

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_get_filename_for_content_segment_doesnt_exist(self, mock1):
        cmd_mock = Mock()
        cmd_mock.rc = 1
        cmd_mock.stdout = ''
        cmd = Mock()
        with patch('gppylib.operations.dump.Command.get_results', return_value=cmd_mock):
            filename = get_filename_for_content(self.context, "metadata", 3, '/data/master', 'remoteHost')
            self.assertEquals(filename, None)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_get_filename_for_content_segment_bad_dir(self, mock1):
        cmd_mock = Mock()
        cmd_mock.rc = 0
        cmd_mock.stdout = ''
        cmd = Mock()
        with patch('gppylib.operations.dump.Command.get_results', return_value=cmd_mock):
            filename = get_filename_for_content(self.context, "metadata", 3, '/tmp', 'remoteHost')
            self.assertEquals(filename, None)

    def test_get_filename_for_content_segment_remote_dir_no_host(self):
        with self.assertRaisesRegexp(Exception, 'Must supply name of remote host to check for metadata file'):
            filename = get_filename_for_content(self.context, "metadata", 3, '/data/master')

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=True)
    def test_convert_report_filename_to_cdatabase_filename_old_format(self, mock1):
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/gp_cdatabase_1_1_20160101010101'
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    def test_convert_report_filename_to_cdatabase_filename_new_format(self, mock1):
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/gp_cdatabase_-1_1_20160101010101'
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    def test_convert_report_filename_to_cdatabase_filename_empty_file(self, mock1):
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/gp_cdatabase_-1_1_20160101010101'
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    def test_convert_report_filename_to_cdatabase_filename_no_report_file(self):
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/gp_cdatabase_-1_1_20160101010101'
        with self.assertRaisesRegexp(Exception, "Unable to locate report file for timestamp"):
            cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=True)
    def test_convert_report_filename_to_cdatabase_filename_with_prefix_old_format(self, mock):
        report_file = '/data/master/db_dumps/20160101/bar_gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/bar_gp_cdatabase_1_1_20160101010101'
        self.context.dump_prefix = 'bar_'
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    def test_convert_report_filename_to_cdatabase_filename_with_prefix_new_format(self, mock):
        report_file = '/data/master/db_dumps/20160101/bar_gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/bar_gp_cdatabase_-1_1_20160101010101'
        self.context.dump_prefix = 'bar_'
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    def test_convert_report_filename_to_cdatabase_filename_with_prefix_empty_file(self, mock):
        report_file = '/data/master/db_dumps/20160101/bar_gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/bar_gp_cdatabase_-1_1_20160101010101'
        self.context.dump_prefix = 'bar_'
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    def test_convert_report_filename_to_cdatabase_filename_ddboost_with_earlier_date(self, mock):
        # use the date from the file to calculate the directory, not the current date
        report_file = '/data/master/db_dumps/20080101/gp_dump_20080101010101.rpt'
        expected_output = '/db_dumps/20080101/gp_cdatabase_-1_1_20080101010101' #path in data domain
        self.context.ddboost = True
        cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)
        self.assertEquals(expected_output, cdatabase_file)

    def test_convert_report_filename_to_cdatabase_filename_with_prefix_no_report_file(self):
        report_file = '/data/master/db_dumps/20160101/bar_gp_dump_20160101010101.rpt'
        expected_output = '/data/master/db_dumps/20160101/bar_gp_cdatabase_-1_1_20160101010101'
        self.context.dump_prefix = 'bar_'
        with self.assertRaisesRegexp(Exception, "Unable to locate report file for timestamp"):
            cdatabase_file = convert_report_filename_to_cdatabase_filename(self.context, report_file)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE DATABASE bkdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    def test_check_cdatabase_exists_default(self, mock1, mock2):
        self.context.target_db = 'bkdb'
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        result = check_cdatabase_exists(self.context, report_file)
        self.assertTrue(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE DATABASE fullbkdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    def test_check_cdatabase_exists_bad_dbname(self, mock):
        self.context.target_db = 'bkdb'
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        result = check_cdatabase_exists(self.context, report_file)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE bkdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    def test_check_cdatabase_exists_no_database(self, mock):
        self.context.target_db = 'bkdb'
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        result = check_cdatabase_exists(self.context, report_file)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test_check_cdatabase_exists_empty_file(self, mock):
        self.context.target_db = 'bkdb'
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        result = check_cdatabase_exists(self.context, report_file)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', 'CREATE DATABASE'])
    def test_check_cdatabase_exists_no_dbname(self, mock):
        self.context.target_db = 'bkdb'
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        result = check_cdatabase_exists(self.context, report_file)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "CREATE DATABASE", "", True, False))
    def test_check_cdatabase_exists_command_result(self, mock1, mock2):
        self.context.target_db = 'bkdb'
        report_file = '/data/master/db_dumps/20160101/gp_dump_20160101010101.rpt'
        self.context.ddboost = True
        result = check_cdatabase_exists(self.context, report_file)
        self.assertFalse(result)

    def test_get_backup_dir_default(self):
        expected = '/data/master/db_dumps/20160101'
        result = self.context.get_backup_dir()
        self.assertTrue(result, expected)

    def test_get_backup_dir_different_backup_dir(self):
        self.context.backup_dir = '/tmp/foo'
        expected = '/tmp/foo/db_dumps/20160101'
        result = self.context.get_backup_dir()
        self.assertTrue(result, expected)

    def test_get_backup_dir_no_mdd(self):
        self.context.master_datadir= None
        with self.assertRaisesRegexp(Exception, 'Cannot locate backup directory with existing parameters'):
            result = self.context.get_backup_dir()

    def test_get_backup_dir_bad_timestamp(self):
        timestamp = 'a0160101010101'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            result = self.context.get_backup_dir(timestamp)

    def test_check_successful_dump_default(self):
        successful_dump = check_successful_dump(['gp_dump utility finished successfully.'])
        self.assertTrue(successful_dump)

    def test_check_successful_dump_failure(self):
        successful_dump = check_successful_dump(['gp_dump utility finished unsuccessfully.'])
        self.assertFalse(successful_dump)

    def test_check_successful_dump_no_result(self):
        successful_dump = check_successful_dump([])
        self.assertFalse(successful_dump)

    def test_check_successful_dump_with_whitespace(self):
        successful_dump = check_successful_dump(['gp_dump utility finished successfully.\n'])
        self.assertTrue(successful_dump)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_default(self, mock1, mock2):
        expected_output = '01234567891234'
        ts = get_full_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=False)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_no_database(self, mock1, mock2):
        expected_output = None
        ts = get_full_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234567', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_timestamp_too_long(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_full_ts_from_report_file(self.context, 'foo')

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: xxx34567891234', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_bad_timestamp(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_full_ts_from_report_file(self.context, 'foo')

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full'])
    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    def test_get_full_ts_from_report_file_missing_output(self, mock1, mock2):
        expected_output = None
        ts = get_full_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_missing_timestamp(self, mock1, mock2):
        expected_output = None
        ts = get_full_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: xxx34567891234', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_with_ddboost_bad_ts(self, mock1, mock2):
        self.context.ddboost = True
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            ts = get_full_ts_from_report_file(self.context, 'foo')

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test_get_full_ts_from_report_file_with_ddboost_good_ts(self, mock1, mock2):
        expected_output = '01234567891234'
        self.context.ddboost = True
        ts = get_full_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test_get_incremental_ts_from_report_file_default(self, mock1, mock2):
        expected_output = '01234567891234'
        ts = get_incremental_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental'])
    def test_get_incremental_ts_from_report_file_missing_output(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'gp_dump utility finished successfully.'])
    def test_get_incremental_ts_from_report_file_success(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test_get_incremental_ts_from_report_file_full(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=False)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test_get_incremental_ts_from_report_file_no_database(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file(self.context, 'foo')
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: 01234567891234567', 'gp_dump utility finished successfully.'])
    def test_get_incremental_ts_from_report_file_timestamp_too_long(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_incremental_ts_from_report_file(self.context, 'foo')

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: xxx34567891234', 'gp_dump utility finished successfully.'])
    def test_get_incremental_ts_from_report_file_bad_timestamp(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_incremental_ts_from_report_file(self.context, 'foo')

    def test_check_backup_type_full(self):
        backup_type = check_backup_type(['Backup Type: Full'], 'Full')
        self.assertEqual(backup_type, True)

    def test_check_backup_type_mismatch(self):
        backup_type = check_backup_type(['Backup Type: Incremental'], 'Full')
        self.assertEqual(backup_type, False)

    def test_check_backup_type_invalid_type(self):
        backup_type = check_backup_type(['foo'], 'Full')
        self.assertEqual(backup_type, False)

    def test_check_backup_type_type_too_long(self):
        backup_type = check_backup_type(['Backup Type: FullQ'], 'Full')
        self.assertEqual(backup_type, False)

    def test_get_timestamp_val_default(self):
        ts_key = get_timestamp_val(['Timestamp Key: 01234567891234'])
        self.assertEqual(ts_key, '01234567891234')

    def test_get_timestamp_val_timestamp_too_short(self):
        ts_key = get_timestamp_val(['Time: 00000'])
        self.assertEqual(ts_key, None)

    def test_get_timestamp_val_bad_timestamp(self):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_timestamp_val(['Timestamp Key: '])

    @patch('os.path.isdir', return_value=True)
    @patch('os.listdir', return_value=['20161212'])
    def test_get_dump_dirs_single(self, mock, mock1):
        self.context.backup_dir = '/tmp'
        expected_output = ['/tmp/db_dumps/20161212']
        ddir = self.context.get_dump_dirs()
        self.assertEqual(ddir, expected_output)

    @patch('os.path.isdir', return_value=True)
    @patch('os.listdir', return_value=['20161212', '20161213', '20161214'])
    def test_get_dump_dirs_multiple(self, mock, mock1):
        self.context.backup_dir = '/tmp'
        expected_output = ['20161212', '20161213', '20161214']
        ddir = self.context.get_dump_dirs()
        self.assertEqual(ddir.sort(), expected_output.sort())

    @patch('os.path.isdir', return_value=True)
    @patch('os.listdir', return_value=[])
    def test_get_dump_dirs_empty(self, mock, mock2):
        self.context.backup_dir = '/tmp'
        self.assertEquals([], self.context.get_dump_dirs())

    @patch('os.path.isdir', return_value=True)
    @patch('os.listdir', return_value=['2016120a', '201612121', 'abcde'])
    def test_get_dump_dirs_bad_dirs(self, mock, mock2):
        self.context.backup_dir = '/tmp'
        self.assertEquals([], self.context.get_dump_dirs())

    @patch('os.listdir', return_value=['11111111', '20161201']) # Second file shouldn't be picked up, pretend it's a file
    @patch('os.path.isdir', side_effect=[True, True, False]) # First value verifies dump dir exists, second and third are for the respective date dirs above
    def test_get_dump_dirs_file_not_dir(self, mock, mock2):
        self.context.backup_dir = '/tmp'
        expected_output = ['/tmp/db_dumps/11111111']
        ddir = self.context.get_dump_dirs()
        self.assertEqual(ddir, expected_output)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20161212', '20161213', '20161214'])
    @patch('os.listdir', return_value=['gp_cdatabase_-1_1_20161212111111', 'gp_dump_20161212000000.rpt', 'gp_cdatabase_-1_1_20161212000001'])
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=['000000'])
    def test_get_latest_full_dump_timestamp_default(self, mock1, mock2, mock3):
        expected_output = ['000000']
        ts = get_latest_full_dump_timestamp(self.context)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=[])
    def test_get_latest_full_dump_timestamp_no_full(self, mock1):
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            get_latest_full_dump_timestamp(self.context)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20161212', '20161213', '20161214'])
    @patch('os.listdir', return_value=['gp_cdatabase_-1_1_2016121211111', 'gp_cdatabase_-1_1_201612120000010', 'gp_cdatabase_-1_1_2016121a111111'])
    def test_get_latest_full_dump_timestamp_bad_timestamp(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            ts = get_latest_full_dump_timestamp(self.context)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20161212', '20161213', '20161214'])
    @patch('os.listdir', return_value=['gp_cdatabase_-1_1_20161212111111', 'gp_dump_20161212000000.rpt.bk', 'gp_cdatabase_-1_1_20161212000001'])
    def test_get_latest_full_dump_timestamp_no_report_file(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            ts = get_latest_full_dump_timestamp(self.context)

    def test_generate_filename_with_ddboost(self):
        expected_output = '/data/master/backup/DCA-35/20160101/gp_dump_20160101010101_last_operation'
        self.context.ddboost = True
        self.context.dump_dir = 'backup/DCA-35'
        output = self.context.generate_filename("last_operation")
        self.assertEquals(output, expected_output)

    def test_generate_filename_with_env_mdd(self):
        timestamp = '20160101010101'
        expected_output = '%s/db_dumps/20160101/gp_dump_20160101010101_ao_state_file' % self.context.master_datadir
        output = self.context.generate_filename("ao")
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20160930'])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', return_value='20160930093000')
    def test_get_latest_report_timestamp_default(self, mock1, mock2):
        self.context.backup_dir = '/foo'
        result = get_latest_report_timestamp(self.context)
        self.assertEquals(result, '20160930093000')

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=[])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', return_value=[])
    def test_get_latest_report_timestamp_no_dirs(self, mock1, mock2):
        self.context.backup_dir = '/foo'
        result = get_latest_report_timestamp(self.context)
        self.assertEquals(result, None)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20160930'])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', return_value=None)
    def test_get_latest_report_timestamp_no_report_file(self, mock1, mock2):
        self.context.backup_dir = '/foo'
        result = get_latest_report_timestamp(self.context)
        self.assertEquals(result, None)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20160930', '20160929'])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', side_effect=[None, '20160929093000'])
    def test_get_latest_report_timestamp_multiple_dirs(self, mock1, mock2):
        self.context.backup_dir = '/foo'
        result = get_latest_report_timestamp(self.context)
        self.assertEquals(result, '20160929093000')

    @patch('os.listdir', return_value=[])
    def test_get_latest_report_in_dir_no_dirs(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.context.dump_prefix)
        self.assertEquals(result, None)

    @patch('os.listdir', return_value=['gp_dump_20130125140013.rpt', 'gp_dump_20160125140013.FOO'])
    def test_get_latest_report_in_dir_bad_extension(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.context.dump_prefix)
        self.assertEquals(result, '20130125140013')

    @patch('os.listdir', return_value=['gp_dump_20130125140013.rpt', 'gp_dump_20160125140013.rpt'])
    def test_get_latest_report_in_dir_different_years(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.context.dump_prefix)
        self.assertEquals(result, '20160125140013')

    @patch('os.listdir', return_value=['gp_dump_20160125140013.rpt', 'gp_dump_20130125140013.rpt'])
    def test_get_latest_report_in_dir_different_years_different_order(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.context.dump_prefix)
        self.assertEquals(result, '20160125140013')

    def test_create_temp_file_with_tables_default(self):
        dirty_tables = ['public.t1', 'public.t2', 'testschema.t3']
        dirty_file = create_temp_file_with_tables(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('table_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_with_tables_no_tables(self):
        dirty_tables = ['']
        dirty_file = create_temp_file_with_tables(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('table_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_from_list_nonstandard_name(self):
        dirty_tables = ['public.t1', 'public.t2', 'testschema.t3']
        dirty_file = create_temp_file_from_list(dirty_tables, 'dirty_hackup_list_')
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_hackup_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_from_list_no_tables_different_name(self):
        dirty_tables = ['']
        dirty_file = create_temp_file_from_list(dirty_tables, 'dirty_hackup_list_')
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_hackup_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_get_timestamp_from_increments_filename_default(self):
        fname = '/data/master/foo/db_dumps/20130207/gp_dump_20130207133000_increments'
        ts = get_timestamp_from_increments_filename(fname, self.context.dump_prefix)
        self.assertEquals(ts, '20130207133000')

    def test_get_timestamp_from_increments_filename_bad_file(self):
        fname = '/data/master/foo/db_dumps/20130207/gpdump_20130207133000_increments'
        with self.assertRaisesRegexp(Exception, 'Invalid increments file'):
            get_timestamp_from_increments_filename(fname, self.context.dump_prefix)

    @patch('glob.glob', return_value=[])
    def test_get_full_timestamp_for_incremental_no_backup(self, mock1):
        self.context.backup_dir = 'home'
        with self.assertRaisesRegexp(Exception, "Could not locate full backup associated with timestamp '20160101010101'. Either increments file or full backup is missing."):
            get_full_timestamp_for_incremental(self.context)

    @patch('glob.glob', return_value=['foo'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test_get_full_timestamp_for_incremental_bad_files(self, mock1, mock2):
        self.context.backup_dir = 'home'
        with self.assertRaisesRegexp(Exception, "Could not locate full backup associated with timestamp '20160101010101'. Either increments file or full backup is missing."):
            get_full_timestamp_for_incremental(self.context)

    @patch('glob.glob', return_value=['/tmp/db_dumps/20130207/gp_dump_20130207093000_increments'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20130207133001', '20130207133000'])
    @patch('os.path.exists', return_value = True)
    def test_get_full_timestamp_for_incremental_default(self, mock1, mock2, mock3):
        self.context.timestamp = '20130207133000'
        full_ts = get_full_timestamp_for_incremental(self.context)
        self.assertEquals(full_ts, '20130207093000')

    def test_check_funny_chars_in_names_exclamation_mark(self):
        tablenames = ['hello! world', 'correct']
        with self.assertRaisesRegexp(Exception, 'Name has an invalid character'):
            check_funny_chars_in_names(tablenames)

    def test_check_funny_chars_in_names_newline(self):
        tablenames = ['hello\nworld', 'propertablename']
        with self.assertRaisesRegexp(Exception, 'Name has an invalid character'):
            check_funny_chars_in_names(tablenames)

    def test_check_funny_chars_in_names_default(self):
        tablenames = ['helloworld', 'propertablename']
        check_funny_chars_in_names(tablenames) #should not raise an exception

    def test_check_funny_chars_in_names_comma(self):
        tablenames = ['hello, world', 'correct']
        with self.assertRaisesRegexp(Exception, 'Name has an invalid character'):
            check_funny_chars_in_names(tablenames)

    def test_expand_partition_tables_do_nothing(self):
        self.assertEqual(expand_partition_tables('foo', None), None)

    @patch('gppylib.operations.backup_utils.dbconn.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('pygresql.pgdb.pgdbCursor.fetchall', return_value=[['public', 'tl1'], ['public', 'tl2']])
    def test_expand_partition_tables_default(self, mock1, mock2, mock3):
        self.context.target_db = 'foo'
        restore_tables = ['public.t1', 'public.t2']
        expected_output = ['public.tl1', 'public.tl2', 'public.t2']
        result = expand_partition_tables(self.context, restore_tables)
        self.assertEqual(result.sort(), expected_output.sort())

    @patch('gppylib.operations.backup_utils.dbconn.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('pygresql.pgdb.pgdbCursor.fetchall', return_value=[])
    def test_expand_partition_tables_no_change(self, mock1, mock2, mock3):
        self.context.target_db = 'foo'
        restore_tables = ['public.t1', 'public.t2']
        expected_output = ['public.t1', 'public.t2']
        result = expand_partition_tables(self.context, restore_tables)
        self.assertEqual(result.sort(), expected_output.sort())

    def test_populate_filter_tables_all_part_tables(self):
        table = 'public.t1'
        rows = [['public', 't1'], ['public', 't2'], ['public', 't3']]
        non_partition_tables = []
        partition_leaves = []
        self.assertEqual(populate_filter_tables(table, rows, non_partition_tables, partition_leaves),
                            (([], ['public.t1', 'public.t2', 'public.t3'])))

    def test_populate_filter_tables_no_part_tables(self):
        table = 'public.t1'
        rows = []
        non_partition_tables = []
        partition_leaves = []
        self.assertEqual(populate_filter_tables(table, rows, non_partition_tables, partition_leaves),
                            ((['public.t1'], [])))

    @patch('gppylib.operations.backup_utils.expand_partition_tables', return_value=['public.t1_p1', 'public.t1_p2', 'public.t1_p3', 'public.t2', 'public.t3'])
    def test_expand_partitions_and_populate_filter_file_part_tables(self, mock):
        dbname = 'bkdb'
        partition_list = ['public.t1', 'public.t2', 'public.t3']
        file_prefix = 'include_dump_tables_file'
        expected_output = ['public.t2', 'public.t3', 'public.t1', 'public.t1_p1', 'public.t1_p2', 'public.t1_p3']
        result = expand_partitions_and_populate_filter_file(dbname, partition_list, file_prefix)
        self.assertTrue(os.path.basename(result).startswith(file_prefix))
        self.assertTrue(os.path.exists(result))
        contents = get_lines_from_file(result)
        self.assertEqual(contents.sort(), expected_output.sort())
        os.remove(result)

    @patch('gppylib.operations.backup_utils.expand_partition_tables', return_value=['public.t1', 'public.t2', 'public.t3'])
    def test_expand_partitions_and_populate_filter_file_no_part_tables(self, mock):
        dbname = 'bkdb'
        partition_list = ['public.t1', 'public.t2', 'public.t3']
        file_prefix = 'exclude_dump_tables_file'
        result = expand_partitions_and_populate_filter_file(dbname, partition_list, file_prefix)
        self.assertTrue(os.path.basename(result).startswith(file_prefix))
        self.assertTrue(os.path.exists(result))
        contents = get_lines_from_file(result)
        self.assertEqual(contents.sort(), partition_list.sort())
        os.remove(result)

    @patch('gppylib.operations.backup_utils.expand_partition_tables', return_value=[])
    def test_expand_partitions_and_populate_filter_file_no_tables(self, mock):
        dbname = 'bkdb'
        partition_list = ['part_table']
        file_prefix = 'exclude_dump_tables_file'
        result = expand_partitions_and_populate_filter_file(dbname, partition_list, file_prefix)
        self.assertTrue(os.path.basename(result).startswith(file_prefix))
        self.assertTrue(os.path.exists(result))
        contents = get_lines_from_file(result)
        self.assertEqual(contents.sort(), partition_list.sort())
        os.remove(result)

    def test_get_batch_from_list_default(self):
        batch = 1000
        length = 3033
        expected = [(0,1000), (1000,2000), (2000,3000), (3000,4000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list_one_job(self):
        batch = 1000
        length = 1
        expected = [(0,1000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list_matching_jobs(self):
        batch = 1000
        length = 1000
        expected = [(0,1000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list_no_jobs(self):
        batch = 1000
        length = 0
        expected = []
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list_more_jobs(self):
        batch = 1000
        length = 2000
        expected = [(0,1000), (1000,2000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    @patch('gppylib.operations.backup_utils.escape_string', side_effect=['public.ao_table', 'public.co_table'])
    def test_list_to_quoted_string_default(self, mock1):
        input = ['public.ao_table', 'public.co_table']
        expected = "'public.ao_table', 'public.co_table'"
        output = list_to_quoted_string(Mock(), input)
        self.assertEqual(expected, output)

    @patch('gppylib.operations.backup_utils.escape_string', side_effect=['   public.ao_table', 'public.co_table   '])
    def test_list_to_quoted_string_whitespace(self, mock1):
        input = ['   public.ao_table', 'public.co_table   ']
        expected = "'   public.ao_table', 'public.co_table   '"
        output = list_to_quoted_string(Mock(), input)
        self.assertEqual(expected, output)

    @patch('gppylib.operations.backup_utils.escape_string', return_value='public.ao_table')
    def test_list_to_quoted_string_one_table(self, mock1):
        input = ['public.ao_table']
        expected = "'public.ao_table'"
        output = list_to_quoted_string(Mock(), input)
        self.assertEqual(expected, output)

    def test_list_to_quoted_string_no_tables(self):
        input = []
        expected = "''"
        output = list_to_quoted_string(None, input)
        self.assertEqual(expected, output)

    def test_generate_filename_with_prefix(self):
        self.context.dump_prefix = 'foo_'
        expected_output = '/data/master/db_dumps/20160101/%sgp_dump_20160101010101.rpt' % self.context.dump_prefix
        output = self.context.generate_filename("report")
        self.assertEquals(output, expected_output)

    def test_generate_filename_with_prefix_and_ddboost(self):
        self.context.dump_prefix = 'foo_'
        expected_output = '/data/master/backup/DCA-35/20160101/%sgp_dump_20160101010101.rpt' % self.context.dump_prefix
        self.context.ddboost = True
        self.context.dump_dir = 'backup/DCA-35'
        output = self.context.generate_filename("report")
        self.assertEquals(output, expected_output)

    @patch('os.listdir', return_value=['bar_gp_dump_20160125140013.rpt', 'foo_gp_dump_20130125140013.rpt'])
    def test_get_latest_report_in_dir_with_mixed_prefixes(self, mock1):
        bdir = '/foo'
        self.context.dump_prefix = 'foo_'
        result = get_latest_report_in_dir(bdir, self.context.dump_prefix)
        self.assertEquals(result, '20130125140013')

    @patch('os.listdir', return_value=['gp_dump_20130125140013.rpt'])
    def test_get_latest_report_in_dir_with_no_prefix(self, mock1):
        bdir = '/foo'
        self.context.dump_prefix = 'foo_'
        result = get_latest_report_in_dir(bdir, self.context.dump_prefix)
        self.assertEquals(result, None)

    @patch('glob.glob', return_value=['/tmp/db_dumps/20130207/foo_gp_dump_20130207093000_increments'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20130207133001', '20130207133000'])
    @patch('os.path.exists', return_value = True)
    def test_get_full_timestamp_for_incremental_with_prefix_default(self, mock1, mock2, mock3):
        self.context.backup_dir = 'home'
        self.context.dump_prefix = 'foo_'
        self.context.timestamp = '20130207133000'
        full_ts = get_full_timestamp_for_incremental(self.context)
        self.assertEquals(full_ts, '20130207093000')

    @patch('glob.glob', return_value=[])
    def test_get_full_timestamp_for_incremental_with_prefix_no_files(self, mock1):
        self.context.backup_dir = 'home'
        self.context.dump_prefix = 'foo_'
        with self.assertRaisesRegexp(Exception, "Could not locate full backup associated with timestamp '20160101010101'. Either increments file or full backup is missing."):
            get_full_timestamp_for_incremental(self.context)

    @patch('glob.glob', return_value=['foo'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test_get_full_timestamp_for_incremental_with_prefix_bad_files(self, mock1, mock2):
        self.context.backup_dir = 'home'
        self.context.dump_prefix = 'foo_'
        with self.assertRaisesRegexp(Exception, "Could not locate full backup associated with timestamp '20160101010101'. Either increments file or full backup is missing."):
            get_full_timestamp_for_incremental(self.context)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=['20161212', '20161213', '20161214'])
    @patch('os.listdir', return_value=['foo_gp_cdatabase_-1_1_20161212111111', 'foo_gp_dump_20161212000000.rpt', 'foo_gp_cdatabase_-1_1_20161212000001'])
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=['000000'])
    def test_get_latest_full_dump_timestamp_with_prefix_multiple_files(self, mock1, mock2, mock3):
        expected_output = ['000000']
        self.context.dump_prefix = 'foo_'
        ts = get_latest_full_dump_timestamp(self.context)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.Context.get_dump_dirs', return_value=[])
    def test_get_latest_full_dump_timestamp_with_prefix_no_backup(self, mock1):
        self.context.dump_prefix = 'foo_'
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            get_latest_full_dump_timestamp(self.context)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_default(self, mock1):
        backup_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_with_segment(self, mock1):
        segment_hostname = "sdw"

        backup_file_with_nbu(self.context, path=self.netbackup_filepath, hostname=segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run', side_effect=Exception('Error backing up file to NetBackup'))
    def test_backup_file_with_nbu_with_Error(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Error backing up file to NetBackup'):
            backup_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_no_block_size(self, mock1):
        self.context.netbackup_block_size = None

        backup_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run', side_effect=Exception('Error backing up file to NetBackup'))
    def test_backup_file_with_nbu_no_block_size_with_error(self, mock1):
        self.context.netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Error backing up file to NetBackup'):
            backup_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_with_keyword(self, mock1):
        netbackup_keyword = "hello"

        backup_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_with_keyword_and_segment(self, mock1):
        netbackup_keyword = "hello"
        segment_hostname = "sdw"

        backup_file_with_nbu(self.context, path=self.netbackup_filepath, hostname=segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_no_block_size_with_keyword_and_segment(self, mock1):
        self.context.netbackup_block_size = None
        segment_hostname = "sdw"
        netbackup_keyword = "hello"

        backup_file_with_nbu(self.context, path=self.netbackup_filepath, hostname=segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_backup_file_with_nbu_with_keyword_and_segment(self, mock1):
        self.context.netbackup_block_size = None
        netbackup_keyword = "hello"

        backup_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_restore_file_with_nbu_no_block_size_with_segment(self, mock1):
        segment_hostname = "sdw"
        self.context.netbackup_block_size = None

        backup_file_with_nbu(self.context, path=self.netbackup_filepath, hostname=segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run', side_effect=Exception('Error backing up file to NetBackup'))
    def test_restore_file_with_nbu_no_block_size_with_segment_and_error(self, mock1):
        segment_hostname = "sdw"
        self.context.netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Error backing up file to NetBackup'):
            restore_file_with_nbu(self.context, path=self.netbackup_filepath, hostname=segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_restore_file_with_nbu_big_block_size(self, mock1):
        self.context.netbackup_block_size = 1024

        restore_file_with_nbu(self.context, path=self.netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test_restore_file_with_nbu_with_segment_and_big_block_size(self, mock1):
        segment_hostname = "sdw"
        self.context.netbackup_block_size = 2048

        restore_file_with_nbu(self.context, path=self.netbackup_filepath, hostname=segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "/tmp/db_dumps/foo", "", True, False))
    def test_check_file_dumped_with_nbu_default(self, mock1, mock2):
        self.assertTrue(check_file_dumped_with_nbu(self.context, path=self.netbackup_filepath))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    def test_check_file_dumped_with_nbu_no_return(self, mock1, mock2):
        self.assertFalse(check_file_dumped_with_nbu(self.context, path=self.netbackup_filepath))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "/tmp/db_dumps/foo", "", True, False))
    def test_check_file_dumped_with_nbu_with_segment(self, mock1, mock2):
        hostname = "sdw"

        self.assertTrue(check_file_dumped_with_nbu(self.context, path=self.netbackup_filepath, hostname=hostname))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    def test_check_file_dumped_with_nbu_with_segment_and_no_return(self, mock1, mock2):
        hostname = "sdw"

        self.assertFalse(check_file_dumped_with_nbu(self.context, path=self.netbackup_filepath, hostname=hostname))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20160701000000', '20160715000000', '20160804000000'])
    def test_get_full_timestamp_for_incremental_with_nbu_default(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        expected_output = '20160701000000'

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20160701000000', '20160715000000'])
    def test_get_full_timestamp_for_incremental_with_nbu_no_full_timestamp(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file')
    def test_get_full_timestamp_for_incremental_with_nbu_empty_file(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000_increments\n/tmp/gp_dump_20160801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20160701000000', '20160715000000'])
    def test_get_full_timestamp_for_incremental_with_nbu_later_timestamp(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000_increments\n/tmp/gp_dump_20160801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20160710000000', '20160720000000', '20160804000000'])
    def test_get_full_timestamp_for_incremental_with_nbu_multiple_increments(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        expected_output = '20160701000000'

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/foo_gp_dump_20160701000000_increments\n/tmp/foo_gp_dump_20160801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20160710000000', '20160720000000', '20160804000000'])
    def test_get_full_timestamp_for_incremental_with_nbu_with_prefix(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        self.context.dump_prefix = 'foo'
        expected_output = '20160701000000'

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/foo_gp_dump_20160701000000_increments\n/tmp/foo_gp_dump_20160801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20160710000000', '20160720000000'])
    def test_get_full_timestamp_for_incremental_with_nbu_no_matching_increment(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.timestamp = '20160804000000'
        self.context.dump_prefix = 'foo'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/data/master/gp_dump_20160701000000.rpt\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value='20160701000000')
    def test_get_latest_full_ts_with_nbu_default(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        expected_output = '20160701000000'

        result = get_latest_full_ts_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/data/master/gp_dump_20160701000000.rpt\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test_get_latest_full_ts_with_nbu_no_full(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(self.context)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test_get_latest_full_ts_with_nbu_no_report_file(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(self.context)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000.rpt\n/tmp/gp_dump_20160720000000.rpt", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test_get_latest_full_ts_with_nbu_empty_report_file(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(self.context)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000.rpt\n/tmp/gp_dump_20160720000000.rpt", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value='20160701000000')
    def test_get_latest_full_ts_with_nbu_default(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        expected_output = '20160701000000'

        result = get_latest_full_ts_with_nbu(self.context)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20160701000000.rpt\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test_get_latest_full_ts_with_nbu_with_prefix(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        self.context.dump_prefix = 'foo'
        expected_output = None

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(self.context)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "No object matched the specified predicate\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test_get_latest_full_ts_with_nbu_no_object(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_block_size = 1024
        expected_output = None

        output = get_latest_full_ts_with_nbu(self.context)
        self.assertEquals(output, expected_output)

    # Yes, this is hackish, but mocking os.environ.get doesn't work.
    def test_init_context_with_no_mdd(self):
        old_mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        try:
            os.environ['MASTER_DATA_DIRECTORY'] = ""
            with self.assertRaisesRegexp(Exception, 'Environment Variable MASTER_DATA_DIRECTORY not set!'):
                context = Context()
        finally:
            os.environ['MASTER_DATA_DIRECTORY'] = old_mdd

    @patch('gppylib.operations.backup_utils.execSQL')
    def test_execute_sql_with_conn(self, execSQL):
        cursor = Mock()
        cursor.fetchall.return_value = 'queryResults'
        execSQL.return_value = cursor

        query = "fake query"
        conn = Mock()
        self.assertEquals('queryResults', execute_sql_with_connection(query, conn))
        execSQL.assert_called_with(conn, query)

    def test__escapeDoubleQuoteInSQLString(self):
        self.assertEqual('MYDATE', escapeDoubleQuoteInSQLString('MYDATE', False))
        self.assertEqual('MY""DATE', escapeDoubleQuoteInSQLString('MY"DATE', False))
        self.assertEqual('MY\'DATE', escapeDoubleQuoteInSQLString('''MY'DATE''', False))
        self.assertEqual('MY""""DATE', escapeDoubleQuoteInSQLString('MY""DATE', False))

        self.assertEqual('"MYDATE"', escapeDoubleQuoteInSQLString('MYDATE'))
        self.assertEqual('"MY""DATE"', escapeDoubleQuoteInSQLString('MY"DATE'))
        self.assertEqual('"MY\'DATE"', escapeDoubleQuoteInSQLString('''MY'DATE'''))
        self.assertEqual('"MY""""DATE"', escapeDoubleQuoteInSQLString('MY""DATE'))


    @patch('os.walk', return_value=[('path', ['dir1', 'dir2'], ['gp_dump_20160101010101.rpt', 'file2', 'gp_dump_20160101010102.rpt']),
                                    ('path2', ['dir3'], ['gp_dump_20160101010103.rpt']),
                                    ('path3', ['dir4', 'dir5'], ['file5', 'gp_dump_20160101010104.rpt'])])
    def test_get_report_files_and_paths_default(self, mock):
        expectedFiles = [('path','gp_dump_20160101010101.rpt'),
                         ('path', 'gp_dump_20160101010102.rpt'),
                         ('path2','gp_dump_20160101010103.rpt'),
                         ('path3','gp_dump_20160101010104.rpt')]
        reportFiles = self.context.get_report_files_and_paths("/tmp")
        self.assertEqual(expectedFiles, reportFiles)

    @patch('os.walk', return_value=[('path', ['dir1', 'dir2'], ['file1', 'file2']),
                                    ('path2', ['dir3'], ['file3']),])
    def test_get_report_files_and_paths_no_report_files(self, mock):
        with self.assertRaisesRegexp(Exception, "No report files located"):
            self.context.get_report_files_and_paths("/dump_dir")

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Compression Program: gzip',
        'segment 0 (dbid 2) Host host Port 5433 Database testdb BackupFile /gp_dump_0_2_20160101010101: Succeeded'])
    def test_get_compress_and_dbname_from_report_file_normal_dbname_compression(self, mock1):
        compress, dbname = self.context.get_compress_and_dbname_from_report_file("report_file_name")
        self.assertTrue(compress)
        self.assertEquals(dbname, 'testdb')

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Compression Program: gzip',
        'segment 0 (dbid 2) Host host Port 5433 Database "test""db" BackupFile /gp_dump_0_2_20160101010101: Succeeded'])
    def test_get_compress_and_dbname_from_report_file_special_dbname_compression(self, mock1):
        compress, dbname = self.context.get_compress_and_dbname_from_report_file("report_file_name")
        self.assertTrue(compress)
        self.assertEquals(dbname, '"test""db"')

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Compression Program: None',
        'segment 0 (dbid 2) Host host Port 5433 Database testdb BackupFile /gp_dump_0_2_20160101010101: Succeeded'])
    def test_get_compress_and_dbname_from_report_file_normal_dbname_no_compression(self, mock1):
        compress, dbname = self.context.get_compress_and_dbname_from_report_file("report_file_name")
        self.assertFalse(compress)
        self.assertEquals(dbname, 'testdb')

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Compression Program: None',
        'segment 0 (dbid 2) Host host Port 5433 Database "test""db" BackupFile /gp_dump_0_2_20160101010101: Succeeded'])
    def test_get_compress_and_dbname_from_report_file_special_dbname_no_compression(self, mock1):
        compress, dbname = self.context.get_compress_and_dbname_from_report_file("report_file_name")
        self.assertFalse(compress)
        self.assertEquals(dbname, '"test""db"')

    @patch('gppylib.operations.backup_utils.get_lines_from_file',
        return_value=['segment 0 (dbid 2) Host host Port 5433 Database testdb BackupFile /gp_dump_0_2_20160101010101: Succeeded'])
    def test_get_compress_and_dbname_from_report_file_no_compression_line_found(self, mock1):
        with self.assertRaisesRegexp(Exception, "Could not determine database name and compression type from report file"):
            self.context.get_compress_and_dbname_from_report_file("report_file_name")

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Compression Program: gzip'])
    def test_get_compress_and_dbname_from_report_file_no_dbname_line_found(self, mock1):
        with self.assertRaisesRegexp(Exception, "Could not determine database name and compression type from report file"):
            self.context.get_compress_and_dbname_from_report_file("report_file_name")
