#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import os
import shutil
import unittest2 as unittest
from gppylib.commands.base import CommandResult
from gppylib.operations.backup_utils import generate_report_filename, validate_timestamp, generate_increments_filename,\
                                            check_cdatabase_exists, check_successful_dump, convert_reportfilename_to_cdatabasefilename,\
                                            get_backup_directory, generate_ao_state_filename, generate_co_state_filename,\
                                            create_temp_file_with_tables, generate_dirtytable_filename, generate_plan_filename,\
                                            generate_metadata_filename, generate_partition_list_filename, verify_lines_in_file,\
                                            write_lines_to_file, get_lines_from_file, get_full_ts_from_report_file, get_incremental_ts_from_report_file,\
                                            check_backup_type, get_timestamp_val, get_dump_dirs, get_latest_full_dump_timestamp, \
                                            generate_pgstatlastoperation_filename, get_latest_report_timestamp, get_latest_report_in_dir, \
                                            create_temp_file_from_list, get_timestamp_from_increments_filename, get_full_timestamp_for_incremental,\
                                            check_funny_chars_in_tablenames, expand_partition_tables, populate_filter_tables, expand_partitions_and_populate_filter_file, \
                                            generate_files_filename, generate_pipes_filename, generate_master_config_filename, generate_segment_config_filename, \
                                            generate_global_prefix, generate_master_dbdump_prefix, get_ddboost_backup_directory, \
                                            generate_master_status_prefix, generate_seg_dbdump_prefix, generate_seg_status_prefix, \
                                            generate_dbdump_prefix, generate_createdb_filename, generate_filter_filename, backup_file_with_nbu, \
                                            restore_file_with_nbu, generate_global_filename, generate_cdatabase_filename, check_file_dumped_with_nbu, \
                                            get_full_timestamp_for_incremental_with_nbu, get_latest_full_ts_with_nbu, generate_schema_filename, get_batch_from_list, list_to_quoted_string

from mock import patch, MagicMock, Mock

class BackupUtilsTestCase(unittest.TestCase):

    def setUp(self):
        self.backup_dir = None
        self.dump_dir = 'db_dumps'
        self.dump_prefix = ''

    def create_backup_dirs(self, top_dir=os.getcwd(), dump_dirs=[]):
        if dump_dirs is None:
            return

        for dump_dir in dump_dirs:
            backup_dir = os.path.join(top_dir, 'db_dumps', dump_dir)
            if not os.path.exists(backup_dir):
                os.makedirs(backup_dir)
                if not os.path.exists(backup_dir):
                    raise Exception('Failed to create directory %s' % backup_dir)

    def remove_backup_dirs(self, top_dir=os.getcwd(), dump_dirs=[]):
        if dump_dirs is None:
            return

        for dump_dir in dump_dirs:
            backup_dir = os.path.join(top_dir, 'db_dumps', dump_dir)
            shutil.rmtree(backup_dir)
            if os.path.exists(backup_dir):
                raise Exception('Failed to remove directory %s' % backup_dir)

    def test_generate_schema_filename_00(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030_schema'
        output = generate_schema_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test_generate_schema_filename_01(self):
        master_data_dir = '/data'
        backup_dir = '/datadomain'
        timestamp = '20120731093030'
        expected_output = '/datadomain/db_dumps/20120731/gp_dump_20120731093030_schema'
        output = generate_schema_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test_generate_schema_filename_02(self):
        master_data_dir = None
        backup_dir = '/datadomain'
        timestamp = '20120731093030'
        expected_output = '/datadomain/db_dumps/20120731/gp_dump_20120731093030_schema'
        output = generate_schema_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test_generate_schema_filename_03(self):
        master_data_dir = None
        timestamp = '20120731093030'
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory with existing parameters'):
            generate_schema_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test_generate_schema_filename_04(self):
        master_data_dir = '/data'
        timestamp = None
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory without timestamp'):
            generate_schema_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test_generate_schema_filename_05(self):
        master_data_dir = '/data'
        timestamp = 'xx120731093030'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_schema_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test_generate_schema_filename_06(self):
        master_data_dir = '/data'
        timestamp = '2012'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_schema_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test00_generate_report_filename(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030.rpt'
        output = generate_report_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test01_generate_report_filename(self):
        master_data_dir = '/data'
        backup_dir = '/datadomain'
        timestamp = '20120731093030'
        expected_output = '/datadomain/db_dumps/20120731/gp_dump_20120731093030.rpt'
        output = generate_report_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test02_generate_report_filename(self):
        master_data_dir = None
        backup_dir = '/datadomain'
        timestamp = '20120731093030'
        expected_output = '/datadomain/db_dumps/20120731/gp_dump_20120731093030.rpt'
        output = generate_report_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test03_generate_report_filename(self):
        master_data_dir = None
        timestamp = '20120731093030'
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory with existing parameters'):
            generate_report_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test04_generate_report_filename(self):
        master_data_dir = '/data'
        timestamp = None
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory without timestamp'):
            generate_report_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test05_generate_report_filename(self):
        master_data_dir = '/data'
        timestamp = 'xx120731093030'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_report_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test06_generate_report_filename(self):
        master_data_dir = '/data'
        timestamp = '2012'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_report_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test07_validate_timestamp(self):
        ts = "20100729093000"
        result = validate_timestamp(ts)
        self.assertTrue(result)

    def test08_validate_timestamp(self):
        ts = "2010072909300"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test09_validate_timestamp(self):
        ts = "201007290930000"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test10_validate_timestamp(self):
        ts = "00000000000000"
        result = validate_timestamp(ts)
        self.assertTrue(result)

    def test11_validate_timestamp(self):
        ts = "0a000000000000"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test12_validate_timestamp(self):
        ts = "0q000000000000"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test13_validate_timestamp(self):
        ts = " 00000000000000"
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test14_validate_timestamp(self):
        ts = "00000000000000 "
        result = validate_timestamp(ts)
        self.assertFalse(result);

    def test15_validate_timestamp(self):
        ts = None
        result = validate_timestamp(ts)
        self.assertFalse(result)

    def test16_generate_increments_filename(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030_increments'
        output = generate_increments_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test17_generate_increments_filename(self):
        master_data_dir = '/data'
        backup_dir = '/datadomain'
        timestamp = '20120731093030'
        expected_output = '/datadomain/db_dumps/20120731/gp_dump_20120731093030_increments'
        output = generate_increments_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test18_generate_increments_filename(self):
        master_data_dir = None
        backup_dir = '/datadomain'
        timestamp = '20120731093030'
        expected_output = '/datadomain/db_dumps/20120731/gp_dump_20120731093030_increments'
        output = generate_increments_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test19_generate_increments_filename(self):
        master_data_dir = None
        timestamp = '20120731093030'
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory with existing parameters'):
            generate_increments_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test20_generate_increments_filename(self):
        master_data_dir = '/data'
        timestamp = None
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory without timestamp'):
            generate_increments_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test21_generate_increments_filename(self):
        master_data_dir = '/data'
        timestamp = 'xx120731093030'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_increments_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test22_generate_increments_filename(self):
        master_data_dir = '/data'
        timestamp = '2012'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_increments_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test23_convert_reportfilename_to_cdatabasefilename(self):
        report_file = '/tmp/foo/foo/gp_dump_20130104133924.rpt'
        expected_output = '/tmp/foo/foo/gp_cdatabase_1_1_20130104133924'
        cdatabase_file = convert_reportfilename_to_cdatabasefilename(report_file, self.dump_prefix)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE DATABASE testdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    def test24_check_cdatabase_exists(self, mock):
        dbname = 'testdb'
        report_file = '/tmp/foo/foo/gp_dump_20130104133924.rpt'
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix)
        self.assertTrue(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE DATABASE testdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    def test25_check_cdatabase_exists(self, mock):
        dbname = 'bkdb'
        report_file = '/tmp/foo/foo/gp_dump_20130104133924.rpt'
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE testdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    def test26_check_cdatabase_exists(self, mock):
        dbname = 'bkdb'
        report_file = '/tmp/foo/foo/gp_dump_20130104133924.rpt'
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test27_check_cdatabase_exists(self, mock):
        dbname = 'bkdb'
        report_file = '/tmp/foo/foo/gp_dump_20130104133924.rpt'
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', 'CREATE DATABASE'])
    def test28_check_cdatabase_exists(self, mock):
        dbname = 'bkdb'
        report_file = '/tmp/foo/foo/gp_dump'
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix)
        self.assertFalse(result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['--', '-- Database creation', '--', '', "CREATE DATABASE testdb WITH TEMPLATE = template0 ENCODING = 'UTF8' OWNER = dcddev;"])
    def test29_check_cdatabase_exists(self, mock):
        dbname = 'testdb'
        report_file = '/tmp/foo/foo/gp_dump_20130104133924.rpt'
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix)
        self.assertTrue(result)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "CREATE DATABASE", "", True, False))
    def test31_check_cdatabase_exists(self, mock1, mock2):
        dbname = 'bkdb'
        report_file = '/tmp/foo/foo/gp_dump'
        ddboost = True
        result = check_cdatabase_exists(dbname, report_file, self.dump_prefix, ddboost)
        self.assertFalse(result)

    def test30_get_backup_directory(self):
        mdd = '/data'
        timestamp = '20121204090000'
        expected = '/data/db_dumps/20121204'
        result = get_backup_directory(mdd, self.backup_dir, self.dump_dir, timestamp)
        self.assertTrue(result, expected)

    def test31_get_backup_directory(self):
        mdd = '/data'
        backup_dir = '/tmp/foo'
        timestamp = '20121204090000'
        expected = '/tmp/foo/db_dumps/20121204'
        result = get_backup_directory(mdd, backup_dir, self.dump_dir, timestamp)
        self.assertTrue(result, expected)

    def test32_get_backup_directory(self):
        mdd = None
        timestamp = '20121204090000'
        expected = '/tmp/foo/db_dumps/20121204'
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory with existing parameters'):
            result = get_backup_directory(mdd, self.backup_dir, self.dump_dir, timestamp)

    def test33_get_backup_directory(self):
        mdd = '/data'
        timestamp = 'a0121204090000'
        expected = '/tmp/foo/db_dumps/20121204'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            result = get_backup_directory(mdd, self.backup_dir, self.dump_dir, timestamp)

    def test34_generate_dirtytable_filename(self):
        mdd = '/data'
        timestamp = '20121204090000'
        expected = '/data/db_dumps/20121204/gp_dump_20121204090000_dirty_list'
        result = generate_dirtytable_filename(mdd, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(expected, result)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['t1', 't2'])
    def test35_verify_lines_in_file(self, mock):
        fname = 'foo'
        expected = ['t1', 't2']

        # failure will raise an exception
        verify_lines_in_file(fname, expected)


    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['t1 ', 't2'])
    def test36_verify_lines_in_file(self, mock):
        fname = 'foo'
        expected = ['t1', 't2']

        with self.assertRaisesRegexp(Exception, 'contents not as expected, suspected IO error'):
            verify_lines_in_file(fname, expected)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[' t1', 't2'])
    def test37_verify_lines_in_file(self, mock):
        fname = 'foo'
        expected = ['t1', 't2']

        with self.assertRaisesRegexp(Exception, 'contents not as expected, suspected IO error'):
            verify_lines_in_file(fname, expected)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['t1'])
    def test38_verify_lines_in_file(self, mock):
        fname = 'foo'
        expected = ['t1', 't2']

        with self.assertRaisesRegexp(Exception, 'contents not as expected, suspected IO error'):
            verify_lines_in_file(fname, expected)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['t1', 't2', 't3'])
    def test39_verify_lines_in_file(self, mock):
        fname = 'foo'
        expected = ['t1', 't2']

        with self.assertRaisesRegexp(Exception, 'contents not as expected, suspected IO error'):
            verify_lines_in_file(fname, expected)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test40_verify_lines_in_file(self, mock):
        fname = 'foo'
        expected = ['t1', 't2']

        with self.assertRaisesRegexp(Exception, 'contents not as expected, suspected IO error'):
            verify_lines_in_file(fname, expected)

    def test41_generate_plan_filename(self):
        mdd = '/data'
        timestamp = '20121204090000'
        expected = '/data/db_dumps/20121204/gp_restore_20121204090000_plan'
        result = generate_plan_filename(mdd, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(expected, result)

    def test42_generate_partition_list_filename(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030_table_list'
        output = generate_partition_list_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test43_generate_metadata_filename(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_1_1_20120731093030.gz'
        output = generate_metadata_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test44_generate_metadata_filename(self):
        master_data_dir = None
        timestamp = '20120731093030'
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory with existing parameters'):
            generate_metadata_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test45_generate_metadata_filename(self):
        master_data_dir = '/data'
        timestamp = None
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory without timestamp'):
            generate_metadata_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test46_generate_metadata_filename(self):
        master_data_dir = '/data'
        timestamp = 'xx120731093030'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_metadata_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test47_generate_metadata_filename(self):
        master_data_dir = '/data'
        timestamp = '2012'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_metadata_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test48_write_lines_to_file(self):
        lines = ['Hello', 'World\n', '  Greenplum   ']
        filename = os.path.join(os.getcwd(), 'abc')
        write_lines_to_file(filename, lines)
        self.assertTrue(os.path.isfile(filename))
        content = get_lines_from_file(filename)
        expected_output = [s.rstrip() for s in lines]
        self.assertEquals(expected_output, content)
        os.remove(filename)

    def test49_write_lines_to_file(self):
        lines = []
        filename = '/this_directory/doesnot/exist'
        with self.assertRaises(IOError):
            write_lines_to_file(filename, lines)

    def test50_write_lines_to_file(self):
        lines = []
        filename = os.path.join(os.getcwd(), 'abc')
        write_lines_to_file(filename, lines)
        self.assertTrue(os.path.exists(filename))
        content = get_lines_from_file(filename)
        self.assertEquals(lines, content)
        os.remove(filename)

    def test51_write_lines_to_file(self):
        lines = ['Hello', 'World', '', 'Greenplum']
        filename = os.path.join(os.getcwd(), 'abc')
        write_lines_to_file(filename, lines)
        self.assertTrue(os.path.isfile(filename))
        content = get_lines_from_file(filename)
        self.assertEquals(lines, content)
        os.remove(filename)

    def test52_check_successful_dump(self):
        successful_dump = check_successful_dump(['gp_dump utility finished successfully.'])
        self.assertTrue(successful_dump)

    def test53_check_successful_dump(self):
        successful_dump = check_successful_dump(['gp_dump utility finished unsuccessfully.'])
        self.assertFalse(successful_dump)

    def test54_check_successful_dump(self):
        successful_dump = check_successful_dump([])
        self.assertFalse(successful_dump)

    def test55_check_successful_dump(self):
        successful_dump = check_successful_dump(['gp_dump utility finished successfully.\n'])
        self.assertTrue(successful_dump)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test56_get_full_ts_from_report_file(self, mock1, mock2):
        expected_output = '01234567891234'
        ts = get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=False)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test57_get_full_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234567', 'gp_dump utility finished successfully.'])
    def test58_get_full_ts_from_report_file(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: xxx34567891234', 'gp_dump utility finished successfully.'])
    def test59_get_full_ts_from_report_file(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix)

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full'])
    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    def test60_get_full_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'gp_dump utility finished successfully.'])
    def test61_get_full_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: xxx34567891234', 'gp_dump utility finished successfully.'])
    def test61_get_full_ts_from_report_file_with_ddboost_bad_ts(self, mock1, mock2):
        ddboost = True
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            ts = get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix, ddboost)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test61_get_full_ts_from_report_file_with_ddboost_good_ts(self, mock1, mock2):
        expected_output = '01234567891234'
        ddboost = True
        ts = get_full_ts_from_report_file('testdb', 'foo', self.dump_prefix, ddboost)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental'])
    def test62_get_incremental_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'gp_dump utility finished successfully.'])
    def test63_get_incremental_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Full', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test64_get_incremental_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test65_get_incremental_ts_from_report_file(self, mock1, mock2):
        expected_output = '01234567891234'
        ts = get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=False)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: 01234567891234', 'gp_dump utility finished successfully.'])
    def test66_get_incremental_ts_from_report_file(self, mock1, mock2):
        expected_output = None
        ts = get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: 01234567891234567', 'gp_dump utility finished successfully.'])
    def test67_get_incremental_ts_from_report_file(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)

    @patch('gppylib.operations.backup_utils.check_cdatabase_exists', return_value=True)
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['Backup Type: Incremental', 'Timestamp Key: xxx34567891234', 'gp_dump utility finished successfully.'])
    def test68_get_incremental_ts_from_report_file(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_incremental_ts_from_report_file('testdb', 'foo', self.dump_prefix)

    def test69_check_backup_type(self):
        backup_type = check_backup_type(['Backup Type: Full'], 'Full')
        self.assertEqual(backup_type, True)

    def test70_check_backup_type(self):
        backup_type = check_backup_type(['Backup Type: Incremental'], 'Full')
        self.assertEqual(backup_type, False)

    def test71_check_backup_type(self):
        backup_type = check_backup_type(['foo'], 'Full')
        self.assertEqual(backup_type, False)

    def test72_check_backup_type(self):
        backup_type = check_backup_type(['Backup Type: FullQ'], 'Full')
        self.assertEqual(backup_type, False)

    def test73_get_timestamp_val(self):
        ts_key = get_timestamp_val(['Timestamp Key: 01234567891234'])
        self.assertEqual(ts_key, '01234567891234')

    def test74_get_timestamp_val(self):
        ts_key = get_timestamp_val(['Time: 00000'])
        self.assertEqual(ts_key, None)

    def test75_get_timestamp_val(self):
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp value found in report_file'):
            get_timestamp_val(['Timestamp Key: '])

    def test76_get_dump_dirs(self):
        dump_dirs = ['20121212']
        self.create_backup_dirs(dump_dirs=dump_dirs)
        expected_output = [os.path.join(os.getcwd(), 'db_dumps', d) for d in dump_dirs]
        try:
            ddir = get_dump_dirs(os.getcwd(), self.dump_dir)
            self.assertEqual(ddir, expected_output)
        finally:
            self.remove_backup_dirs(dump_dirs=dump_dirs)

    def test77_get_dump_dirs(self):
        dump_dir_list = ['20121212', '20121213', '20121214']
        expected_output = [os.path.join(os.getcwd(), 'db_dumps', d) for d in dump_dir_list]
        self.create_backup_dirs(dump_dirs=dump_dir_list)
        try:
            ddir = get_dump_dirs(os.getcwd(), self.dump_dir)
            self.assertEqual(ddir.sort(), expected_output.sort())
        finally:
            self.remove_backup_dirs(dump_dirs=dump_dir_list)

    def test78_get_dump_dirs(self):
        dump_dir_list = []
        self.create_backup_dirs(dump_dirs=dump_dir_list)
        try:
            self.assertEquals([], get_dump_dirs(os.getcwd(), self.dump_dir))
        finally:
            self.remove_backup_dirs(dump_dirs=dump_dir_list)

    def test79_get_dump_dirs(self):
        dump_dir_list = ['2012120a', '201212121', 'abcde']
        self.create_backup_dirs(dump_dirs=dump_dir_list)
        try:
            self.assertEquals([], get_dump_dirs(os.getcwd(), self.dump_dir))
        finally:
            self.remove_backup_dirs(dump_dirs=dump_dir_list)

    def test80_get_dump_dirs(self):
        dump_dir_list = ['11111111']
        expected_output = [os.path.join(os.getcwd(), 'db_dumps', d) for d in dump_dir_list]
        self.create_backup_dirs(dump_dirs=dump_dir_list)

        # this file should not get picked up
        fn = os.path.join(os.getcwd(), 'db_dumps', '20121201')
        with open(fn, 'w') as fd:
            fd.write('hello world')

        try:
            ddir = get_dump_dirs(os.getcwd(), self.dump_dir)
            self.assertEqual(ddir, expected_output)
        finally:
            self.remove_backup_dirs(dump_dirs=dump_dir_list)
            os.remove(fn)

    def test81_get_dump_dirs(self):
        dump_dirs = None
        self.create_backup_dirs(dump_dirs=None)
        try:
            self.assertEquals([], get_dump_dirs(os.getcwd(), self.dump_dir))
        finally:
            self.remove_backup_dirs(dump_dirs=None)

    def test82_get_dump_dirs(self):
        self.assertEquals([], get_dump_dirs('abcdef', self.dump_dir))

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20121212', '20121213', '20121214'])
    @patch('os.listdir', return_value=['gp_cdatabase_1_1_20121212111111', 'gp_dump_20121212000000.rpt', 'gp_cdatabase_1_1_20121212000001'])
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=['000000'])
    def test83_get_latest_full_dump_timestamp(self, mock1, mock2, mock3):
        expected_output = ['000000']
        ts = get_latest_full_dump_timestamp('testdb', '/foo/db_dumps', self.dump_dir, self.dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=[])
    def test84_get_latest_full_dump_timestamp(self, mock1):
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            get_latest_full_dump_timestamp('testdb', '/foo/db_dumps', self.dump_dir, self.dump_prefix)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20121212', '20121213', '20121214'])
    @patch('os.listdir', return_value=[])
    def test85_get_latest_full_dump_timestamp(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid None param to get_latest_full_dump_timestamp'):
            get_latest_full_dump_timestamp('testdb', None, self.dump_dir, self.dump_prefix)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20121212', '20121213', '20121214'])
    @patch('os.listdir', return_value=['gp_cdatabase_1_1_2012121211111', 'gp_cdatabase_1_1_201212120000010', 'gp_cdatabase_1_1_2012121a111111'])
    def test86_get_latest_full_dump_timestamp(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            ts = get_latest_full_dump_timestamp('testdb', '/tmp/foo', self.dump_dir, self.dump_prefix)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20121212', '20121213', '20121214'])
    @patch('os.listdir', return_value=['gp_cdatabase_1_1_20121212111111', 'gp_dump_20121212000000.rpt.bk', 'gp_cdatabase_1_1_20121212000001'])
    def test87_get_latest_full_dump_timestamp(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            ts = get_latest_full_dump_timestamp('testdb', '/tmp/foo', self.dump_dir, self.dump_prefix)

    def test88_generate_pgstatlastoperation_filename(self):
        master_data_dir = '/data'
        backup_dir = None
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030_last_operation'
        output = generate_pgstatlastoperation_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test88_generate_pgstatlastoperation_filename_with_ddboost(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/backup/DCA-35/20120731/gp_dump_20120731093030_last_operation'
        ddboost = True
        dump_dir = 'backup/DCA-35'
        output = generate_pgstatlastoperation_filename(master_data_dir, self.backup_dir, dump_dir, self.dump_prefix, timestamp, ddboost)
        self.assertEquals(output, expected_output)

    def test89_generate_ao_state_filename(self):
        master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        timestamp = '20120731093030'
        expected_output = '%s/db_dumps/20120731/gp_dump_20120731093030_ao_state_file' % master_data_dir
        output = generate_ao_state_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEqual(output, expected_output)

    def test90_generate_ao_state_filename(self):
        master_data_dir = None
        backup_dir = '/tmp'
        timestamp = '20120731093030'
        expected_output = '%s/db_dumps/20120731/gp_dump_20120731093030_ao_state_file' % backup_dir
        output = generate_ao_state_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEqual(output, expected_output)

    def test91_generate_ao_state_filename_with_ddboost(self):
        master_data_dir = '/data'
        backup_dir = '/tmp'
        timestamp = '20120731093030'
        expected_output = '/data/backup/DCA-35/20120731/gp_dump_20120731093030_ao_state_file'
        ddboost = True
        dump_dir = 'backup/DCA-35'
        output = generate_ao_state_filename(master_data_dir, backup_dir, dump_dir, self.dump_prefix, timestamp, ddboost)
        self.assertEqual(output, expected_output)

    def test91_generate_co_state_filename(self):
        master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        timestamp = '20120731093030'
        expected_output = '%s/db_dumps/20120731/gp_dump_20120731093030_co_state_file' % master_data_dir
        output = generate_co_state_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEqual(output, expected_output)

    def test92_generate_co_state_filename(self):
        master_data_dir = None
        backup_dir = '/tmp'
        timestamp = '20120731093030'
        expected_output = '%s/db_dumps/20120731/gp_dump_20120731093030_co_state_file' % backup_dir
        output = generate_co_state_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEqual(output, expected_output)

    def test93_generate_co_state_filename_with_ddboost(self):
        master_data_dir = '/data'
        backup_dir = '/tmp'
        timestamp = '20120731093030'
        expected_output = '/data/backup/DCA-35/20120731/gp_dump_20120731093030_co_state_file'
        ddboost = True
        dump_dir = 'backup/DCA-35'
        output = generate_co_state_filename(master_data_dir, backup_dir, dump_dir, self.dump_prefix, timestamp, ddboost)
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=[])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', return_value=[])
    def test93_get_latest_report_timestamp(self, mock1, mock2):
        bdir = '/foo'
        result = get_latest_report_timestamp(bdir, self.dump_dir, self.dump_prefix)
        self.assertEquals(result, None)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20120930'])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', return_value=None)
    def test94_get_latest_report_timestamp(self, mock1, mock2):
        bdir = '/foo'
        result = get_latest_report_timestamp(bdir, self.dump_dir, self.dump_prefix)
        self.assertEquals(result, None)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20120930'])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', return_value='20120930093000')
    def test95_get_latest_report_timestamp(self, mock1, mock2):
        bdir = '/foo'
        result = get_latest_report_timestamp(bdir, self.dump_dir, self.dump_prefix)
        self.assertEquals(result, '20120930093000')

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20120930', '20120929'])
    @patch('gppylib.operations.backup_utils.get_latest_report_in_dir', side_effect=[None, '20120929093000'])
    def test96_get_latest_report_timestamp(self, mock1, mock2):
        bdir = '/foo'
        result = get_latest_report_timestamp(bdir, self.dump_dir, self.dump_prefix)
        self.assertEquals(result, '20120929093000')

    @patch('os.listdir', return_value=[])
    def test97_get_latest_report_in_dir(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.dump_prefix)
        self.assertEquals(result, None)

    @patch('os.listdir', return_value=['gp_dump_20130125140013.rpt', 'gp_dump_20140125140013.FOO'])
    def test98_get_latest_report_in_dir(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.dump_prefix)
        self.assertEquals(result, '20130125140013')

    @patch('os.listdir', return_value=['gp_dump_20130125140013.rpt', 'gp_dump_20140125140013.rpt'])
    def test99_get_latest_report_in_dir(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.dump_prefix)
        self.assertEquals(result, '20140125140013')

    @patch('os.listdir', return_value=['gp_dump_20140125140013.rpt', 'gp_dump_20130125140013.rpt'])
    def test100(self, mock1):
        bdir = '/foo'
        result = get_latest_report_in_dir(bdir, self.dump_prefix)
        self.assertEquals(result, '20140125140013')

    def test_create_temp_file_with_tables_00(self):
        dirty_tables = ['public.t1', 'public.t2', 'pepper.t3']
        dirty_file = create_temp_file_with_tables(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('table_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_with_tables_01(self):
        dirty_tables = ['']
        dirty_file = create_temp_file_with_tables(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('table_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_with_tables_02(self):
        dirty_tables = ['']
        dirty_file = create_temp_file_with_tables(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('table_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_from_list_00(self):
        dirty_tables = ['public.t1', 'public.t2', 'pepper.t3']
        dirty_file = create_temp_file_from_list(dirty_tables, 'dirty_hackup_list_')
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_hackup_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_from_list_01(self):
        dirty_tables = ['']
        dirty_file = create_temp_file_from_list(dirty_tables, 'dirty_hackup_list_')
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_hackup_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_temp_file_from_list_02(self):
        dirty_tables = ['']
        dirty_file = create_temp_file_from_list(dirty_tables, 'dirty_hackup_list_')
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_hackup_list'))
        self.assertTrue(os.path.exists(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_get_timestamp_from_increments_filename_00(self):
        fname = '/data/foo/db_dumps/20130207/gp_dump_20130207133000_increments'
        ts = get_timestamp_from_increments_filename(fname, self.dump_prefix)
        self.assertEquals(ts, '20130207133000')

    def test_get_timestamp_from_increments_filename_01(self):
        fname = '/data/foo/db_dumps/20130207/gpdump_20130207133000_increments'
        with self.assertRaisesRegexp(Exception, 'Invalid increments file'):
            get_timestamp_from_increments_filename(fname, self.dump_prefix)

    @patch('glob.glob', return_value=[])
    def test_get_full_timestamp_for_incremental_00(self, m1):
        backup_dir = 'home'
        ts = '20130207133000'
        full_ts = get_full_timestamp_for_incremental(backup_dir, self.dump_dir, self.dump_prefix, ts)
        self.assertEquals(full_ts, None)

    @patch('glob.glob', return_value=['foo'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test_get_full_timestamp_for_incremental_01(self, m1, m2):
        backup_dir = 'home'
        ts = '20130207133000'
        full_ts = get_full_timestamp_for_incremental(backup_dir, self.dump_dir, self.dump_prefix, ts)
        self.assertEquals(full_ts, None)

    @patch('glob.glob', return_value=['/tmp/db_dumps/20130207/gp_dump_20130207093000_increments'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20130207133001', '20130207133000'])
    def test_get_full_timestamp_for_incremental_02(self, m1, m2):
        backup_dir = 'home'
        ts = '20130207133000'
        full_ts = get_full_timestamp_for_incremental(backup_dir, self.dump_dir, self.dump_prefix, ts)
        self.assertEquals(full_ts, '20130207093000')

    def test_check_funny_chars_in_tablenames_00(self):
        tablenames = ['hello, world', 'correct']
        with self.assertRaisesRegexp(Exception, 'Tablename has an invalid character'):
            check_funny_chars_in_tablenames(tablenames)

    def test_check_funny_chars_in_tablenames_01(self):
        tablenames = ['hello\nworld', 'propertablename']
        with self.assertRaisesRegexp(Exception, 'Tablename has an invalid character'):
            check_funny_chars_in_tablenames(tablenames)

    def test_check_funny_chars_in_tablenames_02(self):
        tablenames = ['hello:world', 'propertablename']
        with self.assertRaisesRegexp(Exception, 'Tablename has an invalid character'):
            check_funny_chars_in_tablenames(tablenames)

    def test_check_funny_chars_in_tablenames_03(self):
        tablenames = ['helloworld', 'propertablename']
        check_funny_chars_in_tablenames(tablenames) #should not raise an exception

    def test_expand_partition_tables_00(self):
        self.assertEqual(expand_partition_tables('foo', None), None)

    @patch('gppylib.operations.backup_utils.dbconn.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('pygresql.pgdb.pgdbCursor.fetchall', return_value=[['public', 'tl1'], ['public', 'tl2']])
    def test_expand_partition_tables_01(self, mock1, mock2, mock3):
        dbname = 'foo'
        restore_tables = ['public.t1', 'public.t2']
        expected_output = ['public.tl1', 'public.tl2', 'public.t2']
        result = expand_partition_tables(dbname, restore_tables)
        self.assertEqual(result.sort(), expected_output.sort())

    @patch('gppylib.operations.backup_utils.dbconn.execSQL')
    @patch('gppylib.operations.backup_utils.dbconn.connect')
    @patch('pygresql.pgdb.pgdbCursor.fetchall', return_value=[])
    def test_expand_partition_tables_02(self, mock1, mock2, mock3):
        dbname = 'foo'
        restore_tables = ['public.t1', 'public.t2']
        expected_output = ['public.t1', 'public.t2']
        result = expand_partition_tables(dbname, restore_tables)
        self.assertEqual(result.sort(), expected_output.sort())

    def test_populate_filter_tables_00(self):
        table = 'public.t1'
        rows = [['public', 't1'], ['public', 't2'], ['public', 't3']]
        non_partition_tables = []
        partition_leaves = []
        self.assertEqual(populate_filter_tables(table, rows, non_partition_tables, partition_leaves),
                            (([], ['public.t1', 'public.t2', 'public.t3'])))

    def test_populate_filter_tables_01(self):
        table = 'public.t1'
        rows = []
        non_partition_tables = []
        partition_leaves = []
        self.assertEqual(populate_filter_tables(table, rows, non_partition_tables, partition_leaves),
                            ((['public.t1'], [])))

    @patch('gppylib.operations.backup_utils.expand_partition_tables', return_value=['public.t1_p1', 'public.t1_p2', 'public.t1_p3', 'public.t2', 'public.t3'])
    def test_expand_partitions_and_populate_filter_file01(self, mock):
        dbname = 'testdb'
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
    def test_expand_partitions_and_populate_filter_file02(self, mock):
        dbname = 'testdb'
        partition_list = ['public.t1', 'public.t2', 'public.t3']
        file_prefix = 'exclude_dump_tables_file'
        result = expand_partitions_and_populate_filter_file(dbname, partition_list, file_prefix)
        self.assertTrue(os.path.basename(result).startswith(file_prefix))
        self.assertTrue(os.path.exists(result))
        contents = get_lines_from_file(result)
        self.assertEqual(contents.sort(), partition_list.sort())
        os.remove(result)

    @patch('gppylib.operations.backup_utils.expand_partition_tables', return_value=[])
    def test_expand_partitions_and_populate_filter_file03(self, mock):
        dbname = 'testdb'
        partition_list = []
        file_prefix = 'exclude_dump_tables_file'
        result = expand_partitions_and_populate_filter_file(dbname, partition_list, file_prefix)
        self.assertTrue(os.path.basename(result).startswith(file_prefix))
        self.assertTrue(os.path.exists(result))
        contents = get_lines_from_file(result)
        self.assertEqual(contents.sort(), partition_list.sort())
        os.remove(result)

    def test_get_batch_from_list00(self):
        batch = 1000
        length = 3033
        expected = [(0,1000), (1000,2000), (2000,3000), (3000,4000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list01(self):
        batch = 1000
        length = 1
        expected = [(0,1000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list02(self):
        batch = 1000
        length = 1000
        expected = [(0,1000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list03(self):
        batch = 1000
        length = 0
        expected = []
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_get_batch_from_list04(self):
        batch = 1000
        length = 2000
        expected = [(0,1000), (1000,2000)]
        indices = get_batch_from_list(length, batch)
        self.assertEqual(expected, indices)

    def test_list_to_quoted_string00(self):
        input = ['public.ao_table', 'public.co_table']
        expected = "'public.ao_table', 'public.co_table'"
        output = list_to_quoted_string(input)
        self.assertEqual(expected, output)

    def test_list_to_quoted_string01(self):
        input = ['   public.ao_table', 'public.co_table   ']
        expected = "'public.ao_table', 'public.co_table'"
        output = list_to_quoted_string(input)
        self.assertEqual(expected, output)

    def test_list_to_quoted_string02(self):
        input = ['public.ao_table']
        expected = "'public.ao_table'"
        output = list_to_quoted_string(input)
        self.assertEqual(expected, output)

    def test_list_to_quoted_string03(self):
        input = []
        expected = "''"
        output = list_to_quoted_string(input)
        self.assertEqual(expected, output)

    def test01_generate_files_filename(self):
        master_data_dir = '/data'
        backup_dir = '/backup'
        timestamp = '20120731093030'
        expected_output = '/backup/db_dumps/20120731/gp_dump_20120731093030_regular_files'
        output = generate_files_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test02_generate_files_filename(self):
        master_data_dir = None
        backup_dir = '/backup'
        timestamp = '20120731093030'
        expected_output = '/backup/db_dumps/20120731/gp_dump_20120731093030_regular_files'
        output = generate_files_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test03_generate_files_filename(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030_regular_files'
        output = generate_files_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test01_generate_pipes_filename(self):
        master_data_dir = '/data'
        backup_dir = '/backup'
        timestamp = '20120731093030'
        expected_output = '/backup/db_dumps/20120731/gp_dump_20120731093030_pipes'
        output = generate_pipes_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test02_generate_pipes_filename(self):
        master_data_dir = None
        backup_dir = '/backup'
        timestamp = '20120731093030'
        expected_output = '/backup/db_dumps/20120731/gp_dump_20120731093030_pipes'
        output = generate_pipes_filename(master_data_dir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test03_generate_pipes_filename(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/gp_dump_20120731093030_pipes'
        output = generate_pipes_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test00_generate_report_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/db_dumps/20120731/%sgp_dump_20120731093030.rpt' % dump_prefix
        output = generate_report_filename(master_data_dir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test01_generate_report_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/backup/DCA-35/20120731/%sgp_dump_20120731093030.rpt' % dump_prefix
        ddboost = True
        dump_dir = 'backup/DCA-35'
        output = generate_report_filename(master_data_dir, self.backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
        self.assertEquals(output, expected_output)

    def test00_generate_ao_state_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_dir = 'db_dumps'
        dump_prefix = 'foo_'
        expected_output = '%s/%s/20120731/%sgp_dump_20120731093030_ao_state_file' % (master_data_dir, dump_dir, dump_prefix)
        output = generate_ao_state_filename(master_data_dir, self.backup_dir, dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test00_generate_co_state_filename_with_prefix(self):
        master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '%s/db_dumps/20120731/%sgp_dump_20120731093030_co_state_file' % (master_data_dir, dump_prefix)
        output = generate_co_state_filename(master_data_dir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEqual(output, expected_output)

    def test00_generate_increments_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/db_dumps/20120731/%sgp_dump_20120731093030_increments' % dump_prefix
        output = generate_increments_filename(master_data_dir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test01_generate_increments_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/backup/DCA-35/20120731/%sgp_dump_20120731093030_increments' % dump_prefix
        ddboost = True
        dump_dir = 'backup/DCA-35'
        output = generate_increments_filename(master_data_dir, self.backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
        self.assertEquals(output, expected_output)

    def test00_generate_pgstatlastoperation_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/db_dumps/20120731/%sgp_dump_20120731093030_last_operation' % dump_prefix
        output = generate_pgstatlastoperation_filename(master_data_dir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test00_generate_dirtytable_filename_with_prefix(self):
        mdd = '/data'
        timestamp = '20121204090000'
        dump_prefix = 'foo_'
        expected = '/data/db_dumps/20121204/%sgp_dump_20121204090000_dirty_list' % dump_prefix
        result = generate_dirtytable_filename(mdd, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(expected, result)

    def test01_generate_dirtytable_filename_with_prefix(self):
        mdd = '/data'
        timestamp = '20121204090000'
        dump_prefix = 'foo_'
        ddboost = True
        dump_dir = 'backup/DCA-35'
        expected = '/data/backup/DCA-35/20121204/%sgp_dump_20121204090000_dirty_list' % dump_prefix
        result = generate_dirtytable_filename(mdd, self.backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
        self.assertEquals(expected, result)

    def test00_generate_plan_filename_with_prefix(self):
        mdd = '/data'
        timestamp = '20121204090000'
        dump_prefix = 'foo_'
        expected = '/data/db_dumps/20121204/%sgp_restore_20121204090000_plan' % dump_prefix
        result = generate_plan_filename(mdd, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(expected, result)

    def test00_generate_metadata_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/db_dumps/20120731/%sgp_dump_1_1_20120731093030.gz' % dump_prefix
        output = generate_metadata_filename(master_data_dir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test00_generate_partition_list_filename_with_prefix(self):
        master_data_dir = '/data'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/data/db_dumps/20120731/%sgp_dump_20120731093030_table_list' % dump_prefix
        output = generate_partition_list_filename(master_data_dir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test01_generate_files_filename_with_prefix(self):
        master_data_dir = '/data'
        backup_dir = '/backup'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/backup/db_dumps/20120731/%sgp_dump_20120731093030_regular_files' % dump_prefix
        output = generate_files_filename(master_data_dir, backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test01_generate_pipes_filename_with_prefix(self):
        master_data_dir = '/data'
        backup_dir = '/backup'
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '/backup/db_dumps/20120731/%sgp_dump_20120731093030_pipes' % dump_prefix
        output = generate_pipes_filename(master_data_dir, backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    @patch('os.listdir', return_value=['bar_gp_dump_20140125140013.rpt', 'foo_gp_dump_20130125140013.rpt'])
    def test00_get_latest_report_in_dir_with_prefix(self, mock1):
        bdir = '/foo'
        dump_prefix = 'foo_'
        result = get_latest_report_in_dir(bdir, dump_prefix)
        self.assertEquals(result, '20130125140013')

    @patch('os.listdir', return_value=['gp_dump_20130125140013.rpt'])
    def test01_get_latest_report_in_dir_with_prefix(self, mock1):
        bdir = '/foo'
        dump_prefix = 'foo_'
        result = get_latest_report_in_dir(bdir, dump_prefix)
        self.assertEquals(result, None)

    @patch('glob.glob', return_value=[])
    def test_get_full_timestamp_for_incremental_with_prefix_00(self, m1):
        backup_dir = 'home'
        ts = '20130207133000'
        dump_prefix = 'foo_'
        full_ts = get_full_timestamp_for_incremental(backup_dir, self.dump_dir, self.dump_prefix, ts)
        self.assertEquals(full_ts, None)

    @patch('glob.glob', return_value=['foo'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=[])
    def test_get_full_timestamp_for_incremental_with_prefix_01(self, m1, m2):
        backup_dir = 'home'
        ts = '20130207133000'
        dump_prefix = 'foo_'
        full_ts = get_full_timestamp_for_incremental(backup_dir, self.dump_dir, self.dump_prefix, ts)
        self.assertEquals(full_ts, None)

    @patch('glob.glob', return_value=['/tmp/db_dumps/20130207/foo_gp_dump_20130207093000_increments'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20130207133001', '20130207133000'])
    def test_get_full_timestamp_for_incremental_with_prefix_02(self, m1, m2):
        backup_dir = 'home'
        ts = '20130207133000'
        dump_prefix = 'foo_'
        full_ts = get_full_timestamp_for_incremental(backup_dir, self.dump_dir, dump_prefix, ts)
        self.assertEquals(full_ts, '20130207093000')

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20121212', '20121213', '20121214'])
    @patch('os.listdir', return_value=['foo_gp_cdatabase_1_1_20121212111111', 'foo_gp_dump_20121212000000.rpt', 'foo_gp_cdatabase_1_1_20121212000001'])
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=['000000'])
    def test00_get_latest_full_dump_timestamp_with_prefix(self, mock1, mock2, mock3):
        expected_output = ['000000']
        dump_prefix = 'foo_'
        ts = get_latest_full_dump_timestamp('testdb', '/foo/db_dumps', self.dump_dir, dump_prefix)
        self.assertEqual(ts, expected_output)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=[])
    def test01_get_latest_full_dump_timestamp_with_prefix(self, mock1):
        dump_prefix = 'foo_'
        with self.assertRaisesRegexp(Exception, 'No full backup found for incremental'):
            get_latest_full_dump_timestamp('testdb', '/foo/db_dumps', self.dump_dir, dump_prefix)

    @patch('gppylib.operations.backup_utils.get_dump_dirs', return_value=['20121212', '20121213', '20121214'])
    @patch('os.listdir', return_value=[])
    def test02_get_latest_full_dump_timestamp_with_prefix(self, mock1, mock2):
        dump_prefix = 'foo_'
        with self.assertRaisesRegexp(Exception, 'Invalid None param to get_latest_full_dump_timestamp'):
            get_latest_full_dump_timestamp('testdb', None, self.dump_dir, dump_prefix)

    def test00_convert_reportfilename_to_cdatabasefilename_with_prefix(self):
        report_file = '/tmp/foo/foo/bar_gp_dump_20130104133924.rpt'
        expected_output = '/tmp/foo/foo/bar_gp_cdatabase_1_1_20130104133924'
        dump_prefix = 'bar_'
        cdatabase_file = convert_reportfilename_to_cdatabasefilename(report_file, dump_prefix)
        self.assertEquals(expected_output, cdatabase_file)

    @patch('gppylib.operations.backup_utils.get_ddboost_backup_directory', return_value='backup/DCA-35')
    def test01_convert_reportfilename_to_cdatabasefilename_with_prefix(self, mock1):
        report_file = '/tmp/foo/foo/bar_gp_dump_20130104133924.rpt'
        expected_output = 'backup/DCA-35/20130104/bar_gp_cdatabase_1_1_20130104133924'
        dump_prefix = 'bar_'
        ddboost = True
        cdatabase_file = convert_reportfilename_to_cdatabasefilename(report_file, dump_prefix, ddboost)
        self.assertEquals(expected_output, cdatabase_file)

    def test00_generate_master_config_filename(self):
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        expected_output = '%sgp_master_config_files_%s.tar' % (dump_prefix, timestamp)
        output = generate_master_config_filename(dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test00_generate_segment_config_filename(self):
        timestamp = '20120731093030'
        dump_prefix = 'foo_'
        segId = 2
        expected_output = '%sgp_segment_config_files_0_%d_%s.tar' % (dump_prefix, segId, timestamp)
        output = generate_segment_config_filename(dump_prefix, segId, timestamp)
        self.assertEquals(output, expected_output)

    def test00_generate_global_prefix(self):
        dump_prefix = 'foo_'
        expected_output = '%sgp_global_1_1_' % (dump_prefix)
        output = generate_global_prefix(dump_prefix)
        self.assertEquals(output, expected_output)

    def test00_generate_master_dbdump_prefix(self):
        dump_prefix = 'foo_'
        expected_output = '%sgp_dump_1_1_' % (dump_prefix)
        output = generate_master_dbdump_prefix(dump_prefix)
        self.assertEquals(output, expected_output)

    def test00_generate_master_status_prefix(self):
        dump_prefix = 'foo_'
        expected_output = '%sgp_dump_status_1_1_' % (dump_prefix)
        output = generate_master_status_prefix(dump_prefix)
        self.assertEquals(output, expected_output)

    def test00_generate_seg_status_prefix(self):
        dump_prefix = 'foo_'
        expected_output = '%sgp_dump_status_0_' % (dump_prefix)
        output = generate_seg_status_prefix(dump_prefix)
        self.assertEquals(output, expected_output)

    def test00_generate_seg_dbdump_prefix(self):
        dump_prefix = 'foo_'
        expected_output = '%sgp_dump_0_' % (dump_prefix)
        output = generate_seg_dbdump_prefix(dump_prefix)
        self.assertEquals(output, expected_output)

    def test00_generate_dbdump_prefix(self):
        dump_prefix = 'foo_'
        expected_output = '%sgp_dump_' % (dump_prefix)
        output = generate_dbdump_prefix(dump_prefix)
        self.assertEquals(output, expected_output)

    def test00_generate_createdb_filename(self):
        dump_prefix = 'foo_'
        master_datadir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/%sgp_cdatabase_1_1_%s' % (dump_prefix, timestamp)
        output = generate_createdb_filename(master_datadir, None, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test01_generate_createdb_filename(self):
        master_datadir = '/data'
        timestamp = '20120731093030'
        ddboost = True
        dump_dir = 'backup/DCA-35'
        dump_prefix = 'foo_'
        expected_output = '%s/%s/20120731/%sgp_cdatabase_1_1_%s' % (master_datadir, dump_dir, dump_prefix, timestamp)
        output = generate_createdb_filename(master_datadir, self.backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
        self.assertEquals(output, expected_output)

    def test_generate_filter_filename(self):
        dump_prefix='foo_'
        master_datadir = '/data'
        timestamp = '20120731093030'
        expected_output = '/data/db_dumps/20120731/%sgp_dump_%s_filter' % (dump_prefix, timestamp)
        output = generate_filter_filename(master_datadir, self.backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    def test_generate_filter_filename_with_backupdir(self):
        dump_prefix='foo_'
        master_datadir = '/data'
        backup_dir = '/bkup'
        timestamp = '20120731093030'
        expected_output = '/bkup/db_dumps/20120731/%sgp_dump_%s_filter' % (dump_prefix, timestamp)
        output = generate_filter_filename(master_datadir, backup_dir, self.dump_dir, dump_prefix, timestamp)
        self.assertEquals(output, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test00_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test01_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = 100
        netbackup_keyword = None
        segment_hostname = "sdw"

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath, segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run', side_effect=Exception('Error backing up file to NetBackup'))
    def test02_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Error backing up file to NetBackup'):
            backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test03_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = None
        segment_hostname = "sdw"
        netbackup_keyword = None

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath, segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test04_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run', side_effect=Exception('Error backing up file to NetBackup'))
    def test05_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = None
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Error backing up file to NetBackup'):
            backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test06_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = 100
        netbackup_keyword = "hello"

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test07_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = 100
        netbackup_keyword = "hello"
        segment_hostname = "sdw"

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath, segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test08_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = None
        segment_hostname = "sdw"
        netbackup_keyword = "hello"

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath, segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test09_backup_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = None
        netbackup_keyword = "hello"

        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test00_restore_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = None

        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test01_restore_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/tmp/db_dumps/foo"
        segment_hostname = "sdw"
        netbackup_block_size = None

        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, netbackup_filepath, segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run', side_effect=Exception('Error backing up file to NetBackup'))
    def test02_restore_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/tmp/db_dumps/foo"
        segment_hostname = "sdw"
        netbackup_block_size = None

        with self.assertRaisesRegexp(Exception, 'Error backing up file to NetBackup'):
            restore_file_with_nbu(netbackup_service_host, netbackup_block_size, netbackup_filepath, segment_hostname)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test03_restore_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/tmp/db_dumps/foo"
        netbackup_block_size = 1024

        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, netbackup_filepath)

    @patch('gppylib.operations.backup_utils.Command.run')
    def test04_restore_file_with_nbu(self, mock1):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/tmp/db_dumps/foo"
        segment_hostname = "sdw"
        netbackup_block_size = 2048

        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, netbackup_filepath, segment_hostname)

    def test00_generate_global_filename(self):
        master_datadir = "/data"
        dump_dir = "db_dumps"
        dump_date = "20140101"
        timestamp = "20140101000000"
        expected_output = "/data/db_dumps/20140101/gp_global_1_1_20140101000000"

        result = generate_global_filename(master_datadir, self.backup_dir, dump_dir, self.dump_prefix, dump_date, timestamp)
        self.assertEquals(result, expected_output)

    def test01_generate_global_filename(self):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        dump_dir = "db_dumps"
        dump_date = "20140101"
        timestamp = "20140101000000"
        expected_output = "/datadomain/db_dumps/20140101/gp_global_1_1_20140101000000"

        result = generate_global_filename(master_datadir, backup_dir, dump_dir, self.dump_prefix, dump_date, timestamp)
        self.assertEquals(result, expected_output)

    def test02_generate_global_filename(self):
        master_datadir = None
        backup_dir = "/datadomain"
        dump_dir = "db_dumps"
        dump_date = "20140101"
        timestamp = "20140101000000"
        expected_output = "/datadomain/db_dumps/20140101/gp_global_1_1_20140101000000"

        result = generate_global_filename(master_datadir, backup_dir, dump_dir, self.dump_prefix, dump_date, timestamp)
        self.assertEquals(result, expected_output)

    def test00_generate_cdatabase_filename(self):
        master_datadir = "/data"
        dump_dir = "db_dumps"
        dump_prefix = ""
        timestamp = "20140101000000"
        expected_output = "/data/db_dumps/20140101/gp_cdatabase_1_1_20140101000000"

        result = generate_cdatabase_filename(master_datadir, self.backup_dir, dump_dir, dump_prefix, timestamp)
        self.assertEquals(result, expected_output)

    def test01_generate_cdatabase_filename(self):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        timestamp = "20140101000000"
        expected_output = "/datadomain/db_dumps/20140101/gp_cdatabase_1_1_20140101000000"

        result = generate_cdatabase_filename(master_datadir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(result, expected_output)

    def test02_generate_cdatabase_filename(self):
        master_datadir = None
        backup_dir = "/datadomain"
        timestamp = "20140101000000"
        expected_output = "/datadomain/db_dumps/20140101/gp_cdatabase_1_1_20140101000000"

        result = generate_cdatabase_filename(master_datadir, backup_dir, self.dump_dir, self.dump_prefix, timestamp)
        self.assertEquals(result, expected_output)

    def test03_generate_cdatabase_filename(self):
        master_datadir = None
        timestamp = "20140101000000"

        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory with existing parameters'):
            generate_cdatabase_filename(master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test04_generate_cdatabase_filename(self):
        master_data_dir = '/data'
        timestamp = None
        with self.assertRaisesRegexp(Exception, 'Can not locate backup directory without timestamp'):
            generate_cdatabase_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test05_generate_cdatabase_filename(self):
        master_data_dir = '/data'
        timestamp = 'xx120731093030'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_cdatabase_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    def test06_generate_cdatabase_filename(self):
        master_data_dir = '/data'
        timestamp = '2012'
        with self.assertRaisesRegexp(Exception, 'Invalid timestamp'):
            generate_cdatabase_filename(master_data_dir, self.backup_dir, self.dump_dir, self.dump_prefix, timestamp)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "/data/gp_dump_1_1_20141201000000", "", True, False))
    def test00_check_file_dumped_with_nbu(self, mock1, mock2):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/data/gp_dump_1_1_20141201000000"

        self.assertTrue(check_file_dumped_with_nbu(netbackup_service_host, netbackup_filepath))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    def test01_check_file_dumped_with_nbu(self, mock1, mock2):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/data/gp_dump_1_1_20141201000000"

        self.assertFalse(check_file_dumped_with_nbu(netbackup_service_host, netbackup_filepath))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "/data/gp_dump_1_1_20141201000000", "", True, False))
    def test02_check_file_dumped_with_nbu(self, mock1, mock2):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/data/gp_dump_1_1_20141201000000"
        hostname = "sdw"

        self.assertTrue(check_file_dumped_with_nbu(netbackup_service_host, netbackup_filepath, hostname))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    def test03_check_file_dumped_with_nbu(self, mock1, mock2):
        netbackup_service_host = "mdw"
        netbackup_filepath = "/data/gp_dump_1_1_20141201000000"
        hostname = "sdw"

        self.assertFalse(check_file_dumped_with_nbu(netbackup_service_host, netbackup_filepath, hostname))

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "Data Domain Hostname:qadd01\nData Domain Boost Username:metro\n \
            Default Backup Directory:/backup/DCA-35\nData Domain default log level:WARNING", "", True, False))
    def test00_get_ddboost_backup_directory(self, mock1, mock2):
        expected_output = '/backup/DCA-35'
        result = get_ddboost_backup_directory()
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "Data Domain Hostname:qadd01\nData Domain Boost Username:metro\n \
            Deffult Backup Directory:/backup/DCA-35\nData Domain default log level:WARNING", "", True, False))
    def test01_get_ddboost_backup_directory(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'from command gpddboost --show-config not in expected format'):
            get_ddboost_backup_directory()

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "Data Domain Hostname:qadd01\nData Domain Boost Username:metro\n \
            Default Backup Directory:\nData Domain default log level:WARNING", "", True, False))
    def test02_get_ddboost_backup_directory(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'DDBOOST default backup directory is not configured. Or the format of the line has changed'):
            get_ddboost_backup_directory()

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20140701000000', '20140715000000', '20140804000000'])
    def test00_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        expected_output = '20140701000000'

        result = get_full_timestamp_for_incremental_with_nbu(self.dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20140701000000', '20140715000000'])
    def test01_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file')
    def test02_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000_increments\n/tmp/gp_dump_20140801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20140701000000', '20140715000000'])
    def test03_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000_increments\n/tmp/gp_dump_20140801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20140710000000', '20140720000000', '20140804000000'])
    def test04_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        expected_output = '20140701000000'

        result = get_full_timestamp_for_incremental_with_nbu(self.dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/foo_gp_dump_20140701000000_increments\n/tmp/foo_gp_dump_20140801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20140710000000', '20140720000000', '20140804000000'])
    def test05_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        dump_prefix = 'foo'
        expected_output = '20140701000000'

        result = get_full_timestamp_for_incremental_with_nbu(dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/foo_gp_dump_20140701000000_increments\n/tmp/foo_gp_dump_20140801000000_increments\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['20140710000000', '20140720000000'])
    def test06_get_full_timestamp_for_incremental_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        incremental_timestamp = '20140804000000'
        dump_prefix = 'foo'
        expected_output = None

        result = get_full_timestamp_for_incremental_with_nbu(self.dump_prefix, incremental_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/data/gp_dump_20140701000000.rpt\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value='20140701000000')
    def test00_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dbname = "testdb"
        backup_dir = "/data"
        expected_output = '20140701000000'

        result = get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/data/gp_dump_20140701000000.rpt\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test01_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dbname = "testdb"
        backup_dir = "/data"

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test02_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dbname = "testdb"
        backup_dir = "/data"

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000.rpt\n/tmp/gp_dump_20140720000000.rpt", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test03_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dbname = "testdb"
        backup_dir = "/data"

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000.rpt\n/tmp/gp_dump_20140720000000.rpt", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value='20140701000000')
    def test04_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dbname = "testdb"
        backup_dir = "/tmp"
        expected_output = '20140701000000'

        result = get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)
        self.assertEquals(result, expected_output)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "/tmp/gp_dump_20140701000000.rpt\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test05_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dump_prefix = 'foo'
        dbname = "testdb"
        backup_dir = "/tmp"
        expected_output = None

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)

    @patch('gppylib.operations.backup_utils.Command.run')
    @patch('gppylib.operations.backup_utils.Command.get_results', return_value=CommandResult(0, "No object matched the specified predicate\n", "", True, False))
    @patch('gppylib.operations.backup_utils.restore_file_with_nbu')
    @patch('gppylib.operations.backup_utils.get_full_ts_from_report_file', return_value=None)
    def test06_get_latest_full_ts_with_nbu(self, mock1, mock2, mock3, mock4):
        netbackup_service_host = "mdw"
        netbackup_block_size = 1024
        dbname = "testdb"
        backup_dir = "/tmp"

        with self.assertRaisesRegexp(Exception, 'No full backup found for given incremental on the specified NetBackup server'):
            get_latest_full_ts_with_nbu(dbname, backup_dir, self.dump_prefix, netbackup_service_host, netbackup_block_size)
