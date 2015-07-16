#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import os
import shutil
import time
import unittest2 as unittest
from datetime import datetime
from gppylib.commands.base import CommandResult
from gppylib.operations.backup_utils import write_lines_to_file
from gppylib.operations.dump import DumpDatabase, DumpGlobal, compare_dict, create_partition_dict, \
                                    get_dirty_heap_tables, get_filename_from_filetype,\
                                    get_partition_list, get_partition_state, get_last_dump_timestamp, get_lines_from_file,\
                                    get_last_state, get_last_operation_data, get_dirty_partition_tables,\
                                    write_dirty_file, write_partition_list_file, write_dirty_file_to_temp, write_state_file,\
                                    get_pgstatlastoperations_dict, get_tables_with_dirty_metadata, compare_metadata,\
                                    CreateIncrementsFile, PostDumpDatabase, get_dirty_tables, validate_current_timestamp, MailEvent,\
                                    validate_modcount, generate_dump_timestamp, get_filter_file, filter_dirty_tables, get_backup_dir, \
                                    update_filter_file, backup_state_files_with_nbu, backup_cdatabase_file_with_nbu, backup_report_file_with_nbu, \
                                    backup_global_file_with_nbu, backup_config_files_with_nbu, backup_report_file_with_ddboost, \
                                    backup_increments_file_with_ddboost, copy_file_to_dd, backup_dirty_file_with_nbu, backup_increments_file_with_nbu, \
                                    backup_partition_list_file_with_nbu, get_include_schema_list_from_exclude_schema, backup_schema_file_with_ddboost, \
                                    update_filter_file_with_dirty_list, TIMESTAMP, TIMESTAMP_KEY, DUMP_DATE
from mock import patch, MagicMock, Mock

class DumpTestCase(unittest.TestCase):

    def setUp(self):
        self.dumper = DumpDatabase(dump_database='testdb',
                                   dump_schema='testschema',
                                   include_dump_tables=[],
                                   exclude_dump_tables=[],
                                   include_dump_tables_file='/tmp/table_list.txt',
                                   exclude_dump_tables_file=None,
                                   backup_dir='/data/master/p1',
                                   free_space_percent=None,
                                   compress=True,
                                   clear_catalog_dumps=False,
                                   encoding=None,
                                   output_options=[],
                                   batch_default=None,
                                   master_datadir='/data/master/p1',
                                   master_port=0,
                                   dump_dir='db_dumps',
                                   dump_prefix='',
                                   include_schema_file=None,
                                   ddboost=False,
                                   netbackup_service_host=None,
                                   netbackup_policy=None,
                                   netbackup_schedule=None,
                                   netbackup_block_size=None,
                                   netbackup_keyword=None,
                                   incremental=False)

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

    def test00_create_dump_line_without_incremental(self):
        output = self.dumper.create_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --no-expand-children -n "\\"testschema\\"" testdb --table-file=/tmp/table_list.txt"""
        self.assertEquals(output, expected_output)

    @patch('gppylib.operations.dump.get_heap_partition_list', return_value=[['123', 'public', 't4'], ['123', 'public', 't5'], ['123', 'pepper', 't6']])
    def test01_get_dirty_heap_tables(self, mock1):
        expected_output = set(['public.t4', 'public.t5', 'pepper.t6'])
        dirty_table_list = get_dirty_heap_tables(1234, 'testdb')
        self.assertEqual(dirty_table_list, expected_output)

    @patch('gppylib.operations.dump.get_heap_partition_list', return_value=[[], ['123', 'public', 't5'], ['123', 'public', 't6']])
    def test02_get_dirty_heap_tables(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Heap tables query returned rows with unexpected number of columns 0'):
            dirty_table_list = get_dirty_heap_tables(1234, 'testdb')

    def test02_write_dirty_file(self):
        dirty_tables = ['t1', 't2', 't3']
        backup_dir = None
        timestamp_key = '20121212010101'
        self.create_backup_dirs(dump_dirs=['20121212'])
        tmpfilename = write_dirty_file(os.getcwd(), dirty_tables, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        with open(tmpfilename) as tmpfile:
            output = tmpfile.readlines()

        output = map(lambda x: x.strip(), output)
        self.assertEqual(dirty_tables, output)
        os.remove(tmpfilename)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test02a_write_dirty_file(self):
        dirty_tables = ['t1', 't2', 't3']
        timestamp_key = '20121212010101'
        backup_dir = '/tmp'
        self.create_backup_dirs(top_dir=backup_dir, dump_dirs=['20121212'])
        tmpfilename = write_dirty_file('/data', dirty_tables, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        with open(tmpfilename) as tmpfile:
            output = tmpfile.readlines()

        output = map(lambda x: x.strip(), output)
        self.assertEqual(dirty_tables, output)
        os.remove(tmpfilename)
        self.remove_backup_dirs(top_dir=backup_dir, dump_dirs=['20121212'])

    def test03_write_dirty_file(self):
        dirty_tables = None
        timestamp_key = '20121212010101'
        backup_dir = None
        self.create_backup_dirs(dump_dirs=['20121212'])
        tmpfilename = write_dirty_file(dirty_tables, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        self.assertEqual(tmpfilename, None)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test03a_write_dirty_file(self):
        dirty_tables = None
        timestamp_key = '20121212010101'
        backup_dir = '/tmp'
        self.create_backup_dirs(top_dir=backup_dir, dump_dirs=['20121212'])
        tmpfilename = write_dirty_file('/data', dirty_tables, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        self.assertEqual(tmpfilename, None)
        self.remove_backup_dirs(top_dir=backup_dir, dump_dirs=['20121212'])

    def test04_write_dirty(self):
        dirty_tables = []
        timestamp_key = '20121212010101'
        backup_dir = None
        self.create_backup_dirs(dump_dirs=['20121212'])
        tmpfilename = write_dirty_file(os.getcwd(), dirty_tables, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        with open(tmpfilename) as tmpfile:
            output = tmpfile.readlines()

        output = map(lambda x: x.strip(), output)
        self.assertEqual(dirty_tables, output)
        os.remove(tmpfilename)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test04a_write_dirty(self):
        dirty_tables = []
        timestamp_key = '20121212010101'
        backup_dir = '/tmp'
        self.create_backup_dirs(top_dir=backup_dir, dump_dirs=['20121212'])
        tmpfilename = write_dirty_file(os.getcwd(), dirty_tables, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix,  timestamp_key)
        with open(tmpfilename) as tmpfile:
            output = tmpfile.readlines()

        output = map(lambda x: x.strip(), output)
        self.assertEqual(dirty_tables, output)
        os.remove(tmpfilename)
        self.remove_backup_dirs(top_dir=backup_dir, dump_dirs=['20121212'])

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20120330120102', '20120330120103'])
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', return_value='20120330120102')
    def test05_validate_increments_file(self, mock1, mock2):
        # expect no exception to die out of this
        CreateIncrementsFile.validate_increments_file('testdb', '/tmp/fn', '/data', None, None, None)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20120330120102', '20120330120103'])
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', side_effect=Exception('invalid timestamp'))
    def test06_validate_increments_file(self, mock1, mock2):

        with self.assertRaisesRegexp(Exception, "Timestamp '20120330120102' from increments file '/tmp/fn' is not a valid increment"):
            CreateIncrementsFile.validate_increments_file('testdb', '/tmp/fn', '/data', None, None, None)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20120330120102', '20120330120103'])
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', return_value=None)
    def test07_validate_increments_file(self, mock1, mock2):

        with self.assertRaisesRegexp(Exception, "Timestamp '20120330120102' from increments file '/tmp/fn' is not a valid increment"):
            CreateIncrementsFile.validate_increments_file('testdb', '/tmp/fn', '/data', None, None, None)

    def test08_CreateIncrementsFile_init(self):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        self.assertEquals(obj.increments_filename, '/data/db_dumps/20121225/gp_dump_20121225000000_increments')

    def test09_CreateIncrementsFile_execute(self):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        obj.increments_filename = os.path.join(os.getcwd(), 'test.increments')
        if os.path.isfile(obj.increments_filename):
            os.remove(obj.increments_filename)
        result = obj.execute()
        self.assertEquals(1, result)
        os.remove(obj.increments_filename)

    def test10_CreateIncrementsFile_execute(self):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        obj.increments_filename = os.path.join(os.getcwd(), 'test.increments')
        with open(obj.increments_filename, 'w') as fd:
            fd.write('20121225100000')
        with self.assertRaisesRegexp(Exception, ".* is not a valid increment"):
            obj.execute()
        os.remove(obj.increments_filename)

    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test11_CreateIncrementsFile_execute(self, mock1):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        obj.increments_filename = os.path.join(os.getcwd(), 'test.increments')
        with open(obj.increments_filename, 'w') as fd:
            fd.write('20121225100000\n')
        result = obj.execute()
        self.assertEquals(2, result)
        os.remove(obj.increments_filename)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    def test12_CreateIncrementsFile_execute(self, mock1):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        obj.increments_filename = os.path.join(os.getcwd(), 'test.increments')
        with self.assertRaisesRegexp(Exception, 'File not written to'):
            result = obj.execute()
        os.remove(obj.increments_filename)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20121225100000'])
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test13_CreateIncrementsFile_execute(self, mock1, mock2):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        obj.increments_filename = os.path.join(os.getcwd(), 'test.increments')
        with open(obj.increments_filename, 'w') as fd:
            fd.write('20121225100000\n')
        with self.assertRaisesRegexp(Exception, 'not written to'):
            result = obj.execute()
        os.remove(obj.increments_filename)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20121225100001', '20121226000000'])
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test14_CreateIncrementsFile_execute(self, mock1, mock2):
        obj = CreateIncrementsFile('testdb', '20121225000000', '20121226000000', '/data', None, self.dumper.dump_dir, self.dumper.dump_prefix, False, None, None)
        obj.increments_filename = os.path.join(os.getcwd(), 'test.increments')
        with open(obj.increments_filename, 'w') as fd:
            fd.write('20121225100000\n')
        with self.assertRaisesRegexp(Exception, 'trouble adding timestamp'):
            result = obj.execute()
        os.remove(obj.increments_filename)

    @patch('gppylib.operations.dump.get_partition_list', return_value=[[123, 'myschema', 't1'], [4444, 'otherschema', 't2'], [992313, 'public', 't3']])
    @patch('gppylib.operations.dump.generate_partition_list_filename', return_value=os.path.join(os.getcwd(), 'test'))
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_write_partition_list_file_00(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '01234567891234'
        dbname = 'bkdb'
        port = '5432'
        partition_list_file = os.path.join(os.getcwd(), 'test')
        write_partition_list_file(master_datadir, backup_dir, timestamp_key, port, dbname, self.dumper.dump_dir, self.dumper.dump_prefix)
        if not os.path.isfile(partition_list_file):
            raise Exception('Partition list file %s not created' % partition_list_file)
        with open(partition_list_file) as fd:
            partition_list_file_contents = fd.read()
        temp_list = partition_list_file_contents.splitlines()
        self.assertEqual(temp_list, ['myschema.t1', 'otherschema.t2', 'public.t3'])
        os.remove(partition_list_file)

    @patch('gppylib.operations.dump.get_partition_list', return_value=[])
    @patch('gppylib.operations.dump.generate_partition_list_filename', return_value=os.path.join(os.getcwd(), 'test'))
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_write_partition_list_file_01(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '01234567891234'
        dbname = 'bkdb'
        port = '5432'
        partition_list_file = os.path.join(os.getcwd(), 'test')
        write_partition_list_file(master_datadir, backup_dir, timestamp_key, port, dbname, self.dumper.dump_dir, self.dumper.dump_prefix)
        if not os.path.isfile(partition_list_file):
            raise Exception('Partition list file %s not created' % partition_list_file)
        with open(partition_list_file) as fd:
            partition_list_file_contents = fd.read()
        temp_list = partition_list_file_contents.splitlines()
        self.assertEqual(temp_list, [])
        os.remove(partition_list_file)

    @patch('gppylib.operations.dump.get_partition_list', return_value=[['t1'], ['t2'], ['t3']])
    @patch('gppylib.operations.dump.generate_partition_list_filename', return_value=os.path.join(os.getcwd(), 'test'))
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_write_partition_list_file_02(self, mock1, mock2, mock3):
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '01234567891234'
        dbname = 'bkdb'
        port = '5432'
        partition_list_file = os.path.join(os.getcwd(), 'test')
        with self.assertRaisesRegexp(Exception, 'Invalid results from query to get all tables'):
            write_partition_list_file(master_datadir, backup_dir, timestamp_key, port, dbname, self.dumper.dump_dir, self.dumper.dump_prefix)

    @patch('gppylib.operations.dump.get_partition_list', return_value=[['t1', 'foo', 'koo'], ['public', 't2'], ['public', 't3']])
    @patch('gppylib.operations.dump.generate_partition_list_filename', return_value=os.path.join(os.getcwd(), 'test'))
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_write_partition_list_file_03(self, mock1, mock2, mock3):
        master_datadir =  'foo'
        backup_dir = None
        timestamp_key = '01234567891234'
        dbname = 'bkdb'
        port = '5432'
        partition_list_file = os.path.join(os.getcwd(), 'test')
        with self.assertRaisesRegexp(Exception, 'Invalid results from query to get all tables'):
            write_partition_list_file(master_datadir, backup_dir, timestamp_key, port, dbname, self.dumper.dump_dir, self.dumper.dump_prefix)

    @patch('gppylib.operations.dump.get_partition_list', return_value=[['t1', 'foo', 'koo'], ['public', 't2'], ['public', 't3']])
    @patch('gppylib.operations.dump.generate_partition_list_filename', return_value=os.path.join(os.getcwd(), 'test'))
    @patch('gppylib.operations.dump.get_filter_file', return_value='/foo')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'public.t2'])
    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['public.t3', 'public.t2'])
    @patch('shutil.copyfile')
    def test_write_partition_list_file_04(self, mock1, mock2, mock3, mock4, mock5, mock6):
        master_datadir =  'foo'
        backup_dir = None
        timestamp_key = '01234567891234'
        dbname = 'bkdb'
        port = '5432'
        partition_list_file = os.path.join(os.getcwd(), 'test')
        with self.assertRaisesRegexp(Exception, 'After writing file .* contents not as expected, suspected IO error'):
            write_partition_list_file(master_datadir, backup_dir, timestamp_key, port, dbname, self.dumper.dump_dir, self.dumper.dump_prefix)

    def test_dirty_file_to_temp_00(self):
        dirty_tables = ['public.t1', 'public.t2', 'pepper.t3']
        dirty_file = write_dirty_file_to_temp(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_backup_list'))
        self.assertTrue(os.path.isfile(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_dirty_file_to_temp_01(self):
        dirty_tables = ['']
        dirty_file = write_dirty_file_to_temp(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_backup_list'))
        self.assertTrue(os.path.isfile(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_dirty_file_to_temp_02(self):
        dirty_tables = ['']
        dirty_file = write_dirty_file_to_temp(dirty_tables)
        self.assertTrue(os.path.basename(dirty_file).startswith('dirty_backup_list'))
        self.assertTrue(os.path.isfile(dirty_file))
        content = get_lines_from_file(dirty_file)
        self.assertEqual(dirty_tables, content)
        os.remove(dirty_file)

    def test_create_dump_outcome_00(self):
        start = datetime(2012, 7, 31, 9, 30, 00)
        end = datetime(2012, 8, 1, 12, 21, 11)
        rc = 5
        expected_outcome = {'timestamp_start': '20120731093000',
                            'time_start': '09:30:00',
                            'time_end': '12:21:11',
                            'exit_status': 5}
        outcome = self.dumper.create_dump_outcome(start, end, rc)
        self.assertTrue(expected_outcome == outcome)

    def test_get_report_dir00(self):
        pdd = PostDumpDatabase(timestamp_start=None,
                               compress=None,
                               backup_dir='/tmp',
                               batch_default=None,
                               master_datadir='/master',
                               master_port=None,
                               dump_dir='db_dumps',
                               dump_prefix='',
                               ddboost=None,
                               netbackup_service_host=None,
                               incremental=False)
        self.assertEquals(pdd.get_report_dir('20120930'), '/tmp/db_dumps/20120930')

    def test_get_report_dir01(self):
        pdd = PostDumpDatabase(timestamp_start=None,
                               compress=None,
                               backup_dir=None,
                               batch_default=None,
                               master_datadir='/master',
                               master_port=None,
                               dump_dir='db_dumps',
                               dump_prefix='',
                               ddboost=None,
                               netbackup_service_host=None,
                               incremental=False)
        self.assertEquals(pdd.get_report_dir('20120930'), '/master/db_dumps/20120930')

    @patch('gppylib.operations.dump.ValidateDumpDatabase.run')
    @patch('gppylib.operations.dump.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.dump.DumpDatabase.create_filter_file')
    def test_execute_00(self, mock1, mock2, mock3, mock4):
        self.dumper.execute()
        # should not raise any exceptions

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.execSQLForSingleton', return_value='100')
    def test_get_partition_state_00(self, mock1, mock2, mock3):
        master_port=5432
        dbname='testdb'
        partition_info = [(123, 'pepper', 't1', 4444), (234, 'pepper', 't2', 5555)]
        expected_output = ['pepper, t1, 100', 'pepper, t2, 100']
        result = get_partition_state(master_port, dbname, 'pg_aoseg', partition_info)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    def test_get_partition_state_01(self, mock1, mock2):
        master_port=5432
        dbname='testdb'
        partition_info = []
        expected_output = []
        result = get_partition_state(master_port, dbname, 'pg_aoseg', partition_info)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.execSQLForSingleton', return_value='10000000000000000')
    def test_get_partition_state_02(self, mock1, mock2, mock3):
        master_port=5432
        dbname='testdb'
        partition_info = [(123, 'pepper', 't1', 4444), (234, 'pepper', 't2', 5555)]
        expected_output = ['pepper, t1, 10000000000000000', 'pepper, t2, 10000000000000000']
        with self.assertRaisesRegexp(Exception, 'Exceeded backup max tuple count of 1 quadrillion rows per table for:'):
            get_partition_state(master_port, dbname, 'pg_aoseg', partition_info)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.execSQLForSingleton', return_value='100')
    def test_get_partition_state_with_more_than_thousand_partition(self, mock1, mock2, mock3):
        master_port=5432
        dbname='testdb'
        partition_info = [(123, 'pepper', 't1', 4444), (234, 'pepper', 't2', 5555)] * 1000
        expected_output = ['pepper, t1, 100', 'pepper, t2, 100'] * 1000
        result = get_partition_state(master_port, dbname, 'pg_aoseg', partition_info)
        self.assertEqual(result, expected_output)

    def test_get_filename_from_filetype_00(self):
        table_type = 'ao'
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        self.create_backup_dirs(dump_dirs=['20121212'])
        expected_output = '%s/db_dumps/20121212/gp_dump_%s_ao_state_file' % (master_datadir, timestamp_key)
        result = get_filename_from_filetype(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        self.assertEqual(result, expected_output)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test_get_filename_from_filetype_01(self):
        table_type = 'co'
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        self.create_backup_dirs(dump_dirs=['20121212'])
        expected_output = '%s/db_dumps/20121212/gp_dump_%s_co_state_file' % (master_datadir, timestamp_key)
        result = get_filename_from_filetype(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)
        self.assertEqual(result, expected_output)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test_get_filename_from_filetype_01_with_ddboost(self):
        table_type = 'ao'
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        self.create_backup_dirs(dump_dirs=['20121212'])
        ddboost = True
        dump_dir = 'backup/DCA-35'
        expected_output = 'foo/backup/DCA-35/20121212/gp_dump_%s_ao_state_file' % (timestamp_key)
        result = get_filename_from_filetype(table_type, master_datadir, backup_dir, dump_dir, self.dumper.dump_prefix, timestamp_key, ddboost)
        self.assertEqual(result, expected_output)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test_get_filename_from_filetype_02(self):
        table_type = 'foo'
        master_datadir = '/foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        with self.assertRaisesRegexp(Exception, 'Invalid table type *'):
            get_filename_from_filetype(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, timestamp_key)

    def test_get_filename_from_filetype_02_with_ddboost(self):
        table_type = 'co'
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        self.create_backup_dirs(dump_dirs=['20121212'])
        ddboost = True
        dump_dir = 'backup/DCA-35'
        expected_output = 'foo/backup/DCA-35/20121212/gp_dump_%s_co_state_file' % (timestamp_key)
        result = get_filename_from_filetype(table_type, master_datadir, backup_dir, dump_dir, self.dumper.dump_prefix, timestamp_key, ddboost)
        self.assertEqual(result, expected_output)
        self.remove_backup_dirs(dump_dirs=['20121212'])

    def test_write_state_file_00(self):
        table_type = 'foo'
        master_datadir = '/foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        partition_list = ['pepper, t1, 100', 'pepper, t2, 100']
        with self.assertRaisesRegexp(Exception, 'Invalid table type *'):
            write_state_file(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, partition_list)

    @patch('gppylib.operations.dump.get_filename_from_filetype', return_value='/tmp/db_dumps/20121212/gp_dump_20121212010101_ao_state_file')
    def test_write_state_file_01(self, mock1):
        table_type = 'ao'
        master_datadir = 'foo'
        backup_dir = None
        timestamp_key = '20121212010101'
        partition_list = ['pepper, t1, 100', 'pepper, t2, 100']
        self.create_backup_dirs(top_dir='/tmp', dump_dirs=['20121212'])
        write_state_file(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, partition_list)
        if not os.path.isfile('/tmp/db_dumps/20121212/gp_dump_20121212010101_ao_state_file'):
            raise Exception('AO state file was not created successfully')
        self.remove_backup_dirs(top_dir='/tmp', dump_dirs=['20121212'])

    @patch('gppylib.operations.dump.execute_sql', return_value=[['public', 'ao_table', 123, 'CREATE', 'table', '2012: 1'], ['pepper', 'co_table', 333, 'TRUNCATE', '', '2033 :1 - 111']])
    def test_get_last_operation_data_00(self, mock):
        output = get_last_operation_data(1, 'foodb')
        expected = ['public,ao_table,123,CREATE,table,2012: 1', 'pepper,co_table,333,TRUNCATE,,2033 :1 - 111']
        self.assertEquals(output, expected)

    @patch('gppylib.operations.dump.execute_sql', return_value=[])
    def test_get_last_operation_data_01(self, mock):
        output = get_last_operation_data(1, 'foodb')
        expected = []
        self.assertEquals(output, expected)

    @patch('gppylib.operations.dump.execute_sql', return_value=[[123, 'table', '2012: 1'], [333, 'TRUNCATE', '', '2033 :1 - 111']])
    def test_get_last_operation_data_02(self, mock):
        with self.assertRaisesRegexp(Exception, 'Invalid return from query'):
            get_last_operation_data(1, 'foodb')

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['pepper, t1, 100', 'pepper, t2, 100'])
    def test_get_last_state_00(self, mock1, mock2, mock3):
        table_type = 'ao'
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        expected_output = ['pepper, t1, 100', 'pepper, t2, 100']
        output = get_last_state(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=False)
    @patch('gppylib.operations.dump.generate_ao_state_filename', return_value='foo')
    def test_get_last_state_01(self, mock1, mock2, mock3):
        table_type = 'ao'
        master_datadir = '/foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        with self.assertRaisesRegexp(Exception, 'ao state file does not exist: foo'):
            get_last_state(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    def test_get_last_state_02(self, mock1, mock2, mock3):
        table_type = 'ao'
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        expected_output = []
        output = get_last_state(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=True)
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    def test_get_last_state_03(self, mock1, mock2, mock3, mock4, mock5):
        table_type = 'ao'
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        expected_output = []
        output = get_last_state(table_type, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEqual(output, expected_output)

    def test_compare_dict_00(self):
        last_dict = {'pepper.t1':'100', 'pepper.t2':'200'}
        curr_dict = {'pepper.t1':'200', 'pepper.t2':'200'}
        expected_output = set(['pepper.t1'])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_compare_dict_01(self):
        last_dict = {'pepper.t1':'100', 'pepper.t2':'200', 'pepper.t3':'300'}
        curr_dict = {'pepper.t1':'100', 'pepper.t2':'100'}
        expected_output = set(['pepper.t2'])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_compare_dict_02(self):
        last_dict = {'pepper.t1':'100', 'pepper.t2':'200'}
        curr_dict = {'pepper.t1':'100', 'pepper.t2':'200', 'pepper.t3':'300'}
        expected_output = set(['pepper.t3'])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_compare_dict_03(self):
        last_dict = {'pepper.t1':'100', 'pepper.t2':'200'}
        curr_dict = {'pepper.t1':'100', 'pepper.t2':'200'}
        expected_output = set([])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_create_partition_dict_00(self):
        partition_list = ['pepper, t1, 100', 'pepper, t2, 200']
        expected_output = {'pepper.t1':'100', 'pepper.t2':'200'}
        result = create_partition_dict(partition_list)
        self.assertEqual(result, expected_output)

    def test_create_partition_dict_01(self):
        partition_list = []
        expected_output = {}
        result = create_partition_dict(partition_list)
        self.assertEqual(result, expected_output)

    def test_create_partition_dict_02(self):
        partition_list = ['pepper t1 100']
        with self.assertRaisesRegexp(Exception, 'Invalid state file format *'):
            create_partition_dict(partition_list)

    @patch('gppylib.operations.dump.generate_increments_filename')
    @patch('gppylib.operations.dump.os.path.isdir', return_value=False)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=False)
    def test_get_last_dump_timestamp_00(self, mock1, mock2, mock3):
        master_datadir = '/foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        result = get_last_dump_timestamp(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)
        self.assertEqual(result, full_timestamp)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20121212121210', '20121212121211'])
    @patch('gppylib.operations.dump.generate_increments_filename')
    @patch('gppylib.operations.dump.os.path.isdir', return_value=True)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    def test_get_last_dump_timestamp_01(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        expected_output = '20121212121211'
        result = get_last_dump_timestamp(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.generate_increments_filename')
    @patch('gppylib.operations.dump.os.path.isdir', return_value=True)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['2012093009300q'])
    def test_get_last_dump_timestamp_02(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        expected = '20120930093000'
        with self.assertRaisesRegexp(Exception, 'get_last_dump_timestamp found invalid ts in file'):
            get_last_dump_timestamp(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)

    @patch('gppylib.operations.dump.generate_increments_filename')
    @patch('gppylib.operations.dump.os.path.isdir', return_value=True)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[' 20120930093000 \n  \n  '])
    def test_get_last_dump_timestamp_03(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        expected = '20120930093000'
        result = get_last_dump_timestamp(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)
        self.assertEqual(result, expected)

    @patch('gppylib.operations.dump.generate_increments_filename')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[' 20120930093000 \n  \n  '])
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=True)
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    @patch('gppylib.operations.dump.os.path.exists', return_value=True)
    def test_get_last_dump_timestamp_04(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        expected = '20120930093000'
        result = get_last_dump_timestamp(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEqual(result, expected)

    @patch('gppylib.operations.dump.generate_increments_filename')
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=False)
    def test_get_last_dump_timestamp_05(self, mock1, mock2):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        expected = '20121212010101'
        result = get_last_dump_timestamp(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEqual(result, expected)

    def test_get_pgstatlastoperations_dict_00(self):
        last_operations = ['public,t1,1234,ALTER,,201212121212:101010']
        last_operations_dict = get_pgstatlastoperations_dict(last_operations)
        expected_output = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010'}
        self.assertEqual(last_operations_dict, expected_output)

    def test_get_pgstatlastoperations_dict_01(self):
        last_operations = ['public,t1,1234,ALTER,,201212121212:101010', 'public,t2,1234,VACCUM,TRUNCATE,201212121212:101015']
        last_operations_dict = get_pgstatlastoperations_dict(last_operations)
        expected_output = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010',
                           ('1234', 'VACCUM'): 'public,t2,1234,VACCUM,TRUNCATE,201212121212:101015'}
        self.assertEqual(last_operations_dict, expected_output)

    def test_get_pgstatlastoperations_dict_02(self):
        last_operations = []
        last_operations_dict = get_pgstatlastoperations_dict(last_operations)
        expected_output = {}
        self.assertEqual(last_operations_dict, expected_output)

    def test_get_pgstatlastoperations_dict_03(self):
        last_operations = ['public,t1,1234,ALTER,,201212121212:101010',  '2345,VACCUM,TRUNCATE,201212121212:101015']
        with self.assertRaisesRegexp(Exception, 'Wrong number of tokens in last_operation data for last backup'):
            get_pgstatlastoperations_dict(last_operations)

    def test_compare_metadata_00(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201212121212:101010']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set())

    def test_compare_metadata_01(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010'}
        cur_metadata = ['public,t1,1234,TRUNCATE,,201212121212:101010']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set(['public.t1']))

    def test_compare_metadata_02(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201212121212:102510']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set(['public.t1']))

    def test_compare_metadata_03(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201212121212:101010','public,t1,1234,TRUNCATE,,201212121212:101010']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set(['public.t1']))

    def test_compare_metadata_04(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201212121212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201212121212:101010,']
        with self.assertRaisesRegexp(Exception, 'Wrong number of tokens in last_operation data for current backup'):
            compare_metadata(old_metadata, cur_metadata)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    def test_get_tables_with_dirty_metadata_00(self, mock1, mock2):
        expected_output = set()
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = []
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510'])
    def test_get_tables_with_dirty_metadata_01(self, mock1, mock2):
        expected_output = set()
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510']
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102511'])
    def test_get_tables_with_dirty_metadata_02(self, mock1, mock2):
        expected_output = set(['pepper.t2'])
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510']
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['pepper,t2,2234,TRUNCATE,,201212121213:102510'])
    def test_get_tables_with_dirty_metadata_03(self, mock1, mock2):
        expected_output = set(['public.t1'])
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510']
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['pepper,t1,2234,TRUNCATE,,201212121213:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510'])
    def test_get_tables_with_dirty_metadata_04(self, mock1, mock2):
        expected_output = set(['pepper.t2', 'public.t3'])
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = ['pepper,t2,1234,ALTER,CHANGE COLUMN,201212121212:102510',
                                'pepper,t2,2234,TRUNCATE,,201212121213:102510',
                                'public,t3,2234,TRUNCATE,,201212121213:102510']
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)


    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['pepper,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510'])
    def test_get_tables_with_dirty_metadata_05(self, mock1, mock2):
        expected_output = set(['public.t1'])
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510']
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20121212010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['pepper,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510'])
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    def test_get_tables_with_dirty_metadata_06(self, mock1, mock2, mock3):
        expected_output = set(['public.t1'])
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201212121212:102510', 'pepper,t2,2234,TRUNCATE,,201212121213:102510']
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        dirty_tables = get_tables_with_dirty_metadata(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, cur_pgstatoperations, netbackup_service_host, netbackup_block_size)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_state', return_value=['pepper, t1, 100', 'pepper, t2, 200'])
    def test_get_dirty_partition_tables_00(self, mock1):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        table_type = 'ao'
        curr_state_partition_list = ['pepper, t3, 300', 'pepper, t1, 200']
        expected_output = set(['pepper.t3', 'pepper.t1'])
        result = get_dirty_partition_tables(table_type, curr_state_partition_list, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.get_last_state', return_value=['pepper, t1, 100', 'pepper, t2, 200'])
    def test_get_dirty_partition_tables_01(self, mock1):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20121212010101'
        table_type = 'ao'
        curr_state_partition_list = ['pepper, t3, 300', 'pepper, t1, 200']
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        expected_output = set(['pepper.t3', 'pepper.t1'])
        result = get_dirty_partition_tables(table_type, curr_state_partition_list, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.get_dirty_heap_tables', return_value=set(['public.heap_table1']))
    @patch('gppylib.operations.dump.get_dirty_partition_tables', side_effect=[set(['public,ao_t1,100', 'public,ao_t2,100']), set(['public,co_t1,100', 'public,co_t2,100'])])
    @patch('gppylib.operations.dump.get_tables_with_dirty_metadata', return_value=set(['public,ao_t3,1234,CREATE,,20121212101010', 'public,co_t3,2345,VACCUM,,20121212101010', 'public,ao_t1,1234,CREATE,,20121212101010']))
    def test_get_dirty_tables_00(self, mock1, mock2, mock3):
        master_port = '5432'
        dbname = 'foo'
        master_datadir = '/foo'
        backup_dir = None
        fulldump_ts = '20121212101010'
        ao_partition_list = []
        co_partition_list = []
        last_operation_data = []
        netbackup_service_host = None
        netbackup_block_size = None
        dirty_tables = get_dirty_tables(master_port, dbname, master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix,
                                        fulldump_ts, ao_partition_list, co_partition_list, last_operation_data, netbackup_service_host, netbackup_block_size)
        expected_output = ['public.heap_table1', 'public.ao_t1', 'public.ao_t2', 'public.co_t1', 'public.co_t2', 'public.ao_t3', 'public.co_t3']
        self.assertEqual(dirty_tables.sort(), expected_output.sort())

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20130127150999')
    def test_validate_current_timestamp_00(self, mock):
        directory = '/foo'
        #no exception
        validate_current_timestamp(directory, self.dumper.dump_dir, self.dumper.dump_prefix, current='20130127151000')

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20130127151000')
    def test_validate_current_timestamp_01(self, mock):
        directory = '/foo'
        with self.assertRaisesRegexp(Exception, 'There is a future dated backup on the system preventing new backups'):
            validate_current_timestamp(directory, self.dumper.dump_dir, self.dumper.dump_prefix, current='20130127151000')

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20130127151001')
    def test_validate_current_timestamp_02(self, mock):
        directory = '/foo'
        with self.assertRaisesRegexp(Exception, 'There is a future dated backup on the system preventing new backups'):
            validate_current_timestamp(directory, self.dumper.dump_dir, self.dumper.dump_prefix, current='20130127151000')

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20140127151001')
    def test_validate_current_timestamp_03(self, mock):
        directory = '/foo'
        with self.assertRaisesRegexp(Exception, 'There is a future dated backup on the system preventing new backups'):
            validate_current_timestamp(directory, self.dumper.dump_dir, self.dumper.dump_prefix, current='20130127151000')

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20120127150999')
    def test_validate_current_timestamp_04(self, mock):
        directory = '/foo'
        #no exception
        validate_current_timestamp(directory, self.dumper.dump_dir, self.dumper.dump_prefix, current='20130127151000')

    def test_validate_modcount_00(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '999999999999999'
        validate_modcount(schemaname, partitionname, tuple_count)

    def test_validate_modcount_01(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '#########'
        with self.assertRaisesRegexp(Exception, 'Can not convert modification count for table.'):
            validate_modcount(schemaname, partitionname, tuple_count)

    def test_validate_modcount_02(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '1+e15'
        with self.assertRaisesRegexp(Exception, 'Can not convert modification count for table.'):
            validate_modcount(schemaname, partitionname, tuple_count)

    def test_validate_modcount_03(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '1000000000000000'
        with self.assertRaisesRegexp(Exception, 'Exceeded backup max tuple count of 1 quadrillion rows per table for:'):
            validate_modcount(schemaname, partitionname, tuple_count)

    def test_generate_dump_timestamp_00(self):
        timestamp = datetime(2013, 02, 04, 10, 10, 10, 10000)
        generate_dump_timestamp(timestamp)
        from gppylib.operations.dump import DUMP_DATE, TIMESTAMP_KEY, TIMESTAMP
        ts_key = timestamp.strftime("%Y%m%d%H%M%S")
        self.assertEqual(timestamp, TIMESTAMP)
        self.assertEqual(ts_key, TIMESTAMP_KEY)
        self.assertEqual(ts_key[0:8], DUMP_DATE)

    def test_generate_dump_timestamp_01(self):
        timestamp = None
        generate_dump_timestamp(timestamp)
        from gppylib.operations.dump import DUMP_DATE, TIMESTAMP_KEY, TIMESTAMP
        self.assertNotEqual(None, TIMESTAMP)
        self.assertNotEqual(None, TIMESTAMP_KEY)
        self.assertNotEqual(None, DUMP_DATE)

    def test_generate_dump_timestamp_02(self):
        timestamp1 = datetime(2013, 02, 04, 10, 10, 10, 10000)
        generate_dump_timestamp(timestamp1)
        timestamp2 = datetime(2013, 03, 04, 10, 10, 10, 10000)
        generate_dump_timestamp(timestamp2)
        from gppylib.operations.dump import DUMP_DATE, TIMESTAMP_KEY, TIMESTAMP
        ts_key = timestamp2.strftime("%Y%m%d%H%M%S")
        self.assertEqual(timestamp2, TIMESTAMP)
        self.assertEqual(ts_key, TIMESTAMP_KEY)
        self.assertEqual(ts_key[0:8], DUMP_DATE)

    def test_generate_dump_timestamp_03(self):
        timestamp1 = datetime(2013, 02, 04, 10, 10, 10, 10000)
        generate_dump_timestamp(timestamp1)
        timestamp2 = datetime(2012, 03, 04, 10, 10, 10, 10000)
        generate_dump_timestamp(timestamp2)
        from gppylib.operations.dump import DUMP_DATE, TIMESTAMP_KEY, TIMESTAMP
        ts_key = timestamp2.strftime("%Y%m%d%H%M%S")
        self.assertEqual(timestamp2, TIMESTAMP)
        self.assertEqual(ts_key, TIMESTAMP_KEY)
        self.assertEqual(ts_key[0:8], DUMP_DATE)

    def test_create_dump_line_00(self):
        self.dumper.include_schema_file = '/tmp/foo'
        output = self.dumper.create_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --no-expand-children -n "\\"testschema\\"" testdb --table-file=/tmp/table_list.txt --schema-file=/tmp/foo"""
        self.assertEquals(output, expected_output)

    def test00_create_dump_line_with_prefix(self):
        self.dumper.dump_prefix = 'foo_'
        output = self.dumper.create_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" testdb --table-file=/tmp/table_list.txt"""
        self.assertEquals(output, expected_output)

    def test_create_dump_line_with_include_file(self):
        self.dumper.dump_prefix = 'metro_'
        self.dumper.include_dump_tables_file = ('bar')
        output = self.dumper.create_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --prefix=metro_ --no-expand-children -n "\\"testschema\\"" testdb --table-file=%s""" % self.dumper.include_dump_tables_file
        self.assertEquals(output, expected_output)

    def test_create_dump_line_with_no_file_args(self):
        self.dumper.dump_prefix = 'metro_'
        self.dumper.include_dump_tables_file = (None,)
        output = self.dumper.create_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --prefix=metro_ --no-expand-children -n "\\"testschema\\"" testdb"""
        self.assertEquals(output, expected_output)

    def test_create_dump_line_with_netbackup_params(self):
        self.dumper.include_dump_tables_file = (None,)
        self.dumper.netbackup_service_host = "mdw"
        self.dumper.netbackup_policy = "test_policy"
        self.dumper.netbackup_schedule = "test_schedule"
        output = self.dumper.create_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --no-expand-children -n "\\"testschema\\"" testdb --netbackup-service-host=mdw --netbackup-policy=test_policy --netbackup-schedule=test_schedule"""
        self.assertEquals(output, expected_output)

    def test_get_backup_dir_with_master_data_dir(self):
        master_datadir = '/foo'
        backup_dir = None
        self.assertEquals('/foo', get_backup_dir(master_datadir, backup_dir))

    def test_get_backup_dir_with_backup_dir(self):
        master_datadir = 'foo'
        backup_dir = '/bkup'
        self.assertEquals('/bkup', get_backup_dir(master_datadir, backup_dir))

    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    @patch('os.path.isfile', return_value=True)
    def test_get_filter_file_file_exists(self, mock1, mock2):
        dump_prefix = 'metro_'
        master_datadir = '/foo'
        backup_dir = None
        dump_database = 'testdb'
        expected_output = '/foo/db_dumps/20130101/metro_gp_dump_20130101010101_filter'
        self.assertEquals(expected_output, get_filter_file(dump_database, master_datadir, backup_dir, self.dumper.dump_dir, dump_prefix))

    @patch('os.path.isfile', return_value=False)
    @patch('gppylib.operations.dump.get_latest_full_ts_with_nbu', return_value='20130101010101')
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=True)
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    def test_get_filter_file_file_exists_on_nbu(self, mock1, mock2, mock3, mock4):
        dump_prefix = 'metro_'
        master_datadir = '/foo'
        backup_dir = None
        dump_database = 'testdb'
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        expected_output = '/foo/db_dumps/20130101/metro_gp_dump_20130101010101_filter'
        self.assertEquals(expected_output, get_filter_file(dump_database, master_datadir, backup_dir, self.dumper.dump_dir, dump_prefix, netbackup_service_host, netbackup_block_size))

    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    @patch('os.path.isfile', return_value=False)
    def test_get_filter_file_file_does_not_exists(self, mock1, mock2):
        dump_prefix = 'metro_'
        master_datadir = '/foo'
        backup_dir = None
        dump_database = 'testdb'
        self.assertEquals(None, get_filter_file(dump_database, master_datadir, backup_dir, self.dumper.dump_dir, dump_prefix))

    def test_update_filter_file_with_dirty_list_00(self):
        filter_file = '/tmp/foo'
        write_lines_to_file(filter_file, ['public.t1'])
        dirty_tables = ['public.t1', 'public.t2']
        expected_output = ['public.t1', 'public.t2']
        update_filter_file_with_dirty_list(filter_file, dirty_tables)
        output = get_lines_from_file(filter_file)
        try:
            self.assertEquals(expected_output, output)
        finally:
            if os.path.exists(filter_file):
                os.remove(filter_file)

    def test_update_filter_file_with_dirty_list_01(self):
        filter_file = '/tmp/foo'
        write_lines_to_file(filter_file, ['public.t1'])
        dirty_tables = ['public.t1']
        expected_output = ['public.t1']
        update_filter_file_with_dirty_list(filter_file, dirty_tables)
        output = get_lines_from_file(filter_file)
        try:
            self.assertEquals(expected_output, output)
        finally:
            if os.path.exists(filter_file):
                os.remove(filter_file)

    def test_update_filter_file_with_dirty_list_02(self):
        filter_file = '/tmp/foo'
        write_lines_to_file(filter_file, ['public.t1', 'public.t2'])
        dirty_tables = ['public.t2']
        expected_output = ['public.t1', 'public.t2']
        update_filter_file_with_dirty_list(filter_file, dirty_tables)
        output = get_lines_from_file(filter_file)
        try:
            self.assertEquals(expected_output, output)
        finally:
            if os.path.exists(filter_file):
                os.remove(filter_file)

    def test_update_filter_file_with_dirty_list_03(self):
        filter_file = '/tmp/foo'
        write_lines_to_file(filter_file, [])
        dirty_tables = ['public.t1', 'public.t2']
        expected_output = ['public.t1', 'public.t2']
        update_filter_file_with_dirty_list(filter_file, dirty_tables)
        output = get_lines_from_file(filter_file)
        try:
            self.assertEquals(expected_output, output)
        finally:
            if os.path.exists(filter_file):
                os.remove(filter_file)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'pepper.t2'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    @patch('gppylib.operations.dump.get_filter_file', return_value='/foo/metro_gp_dump_20130101010101_filter')
    def test_filter_dirty_tables_with_filter(self, mock1, mock2, mock3):
        dump_prefix = 'metro_'
        master_datadir = '/foo'
        backup_dir = None
        dump_database = 'testdb'
        dirty_tables = ['public.t1', 'public.t2', 'pepper.t1', 'pepper.t2']
        expected_output = ['public.t1', 'pepper.t2']
        self.assertEquals(sorted(expected_output), sorted(filter_dirty_tables(dirty_tables, dump_database, master_datadir, backup_dir, self.dumper.dump_dir, dump_prefix)))

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'pepper.t2'])
    @patch('gppylib.operations.dump.get_filter_file', return_value='/foo/metro_gp_dump_20130101010101_filter')
    @patch('gppylib.operations.dump.get_latest_full_ts_with_nbu', return_value='20130101010101')
    def test_filter_dirty_tables_with_filter_with_nbu(self, mock1, mock2, mock3):
        dump_prefix = 'metro_'
        master_datadir = '/foo'
        backup_dir = None
        dump_database = 'testdb'
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        dirty_tables = ['public.t1', 'public.t2', 'pepper.t1', 'pepper.t2']
        expected_output = ['public.t1', 'pepper.t2']
        self.assertEquals(sorted(expected_output), sorted(filter_dirty_tables(dirty_tables, dump_database, master_datadir, backup_dir, self.dumper.dump_dir, dump_prefix, netbackup_service_host, netbackup_block_size)))

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'pepper.t2'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_filter_dirty_tables_without_filter(self, mock1, mock2, mock3):
        dump_prefix = 'metro_'
        master_datadir = '/foo'
        backup_dir = None
        dump_database = 'testdb'
        dirty_tables = ['public.t1', 'public.t2', 'pepper.t1', 'pepper.t2']
        self.assertEquals(sorted(dirty_tables), sorted(filter_dirty_tables(dirty_tables, dump_database, master_datadir, backup_dir, self.dumper.dump_dir, dump_prefix)))

    @patch('gppylib.operations.dump.get_filter_file', return_value='/tmp/db_dumps/20121212/foo_gp_dump_01234567891234_filter')
    def test_create_filtered_dump_string(self, mock1):
        self.dumper.dump_prefix = 'foo_'
        output = self.dumper.create_filtered_dump_string('dcddev', '20121212', '01234567891234')
        expected_output = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" testdb --table-file=/tmp/table_list.txt --incremental-filter=/tmp/db_dumps/20121212/foo_gp_dump_01234567891234_filter"""
        self.assertEquals(output, expected_output)

    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.dump.Command.run')
    def test_perform_dump_normal(self, mock1, mock2):
        self.dumper.dump_prefix = 'foo_'
        title = 'Dump process'
        dump_line = """gp_dump -p 0 -U dcddev --gp-d=/data/master/p1/db_dumps/20121212 --gp-r=/data/master/p1/db_dumps/20121212 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" testdb --table-file=/tmp/table_list.txt"""
        (start, end, rc) = self.dumper.perform_dump(title, dump_line)
        self.assertNotEqual(start, None)
        self.assertNotEqual(end, None)
        self.assertEquals(rc, 0)

class DumpGlobalTestCase(unittest.TestCase):

    def setUp(self):
        self.dumper = DumpGlobal(timestamp=None,
                                 master_datadir='/foo',
                                 master_port=0,
                                 backup_dir='/foo',
                                 dump_dir='db_dumps',
                                 dump_prefix='',
                                 ddboost=False)

    def test_create_pgdump_command_line(self):
        self.dumper = DumpGlobal(timestamp=TIMESTAMP_KEY, master_datadir='/foo', master_port=9000, backup_dir='/foo', dump_dir='db_dumps', dump_prefix='', ddboost=False)
        global_file_name = '/foo/db_dumps/%s/gp_global_1_1_%s' % (DUMP_DATE, TIMESTAMP_KEY)

        expected_output = "pg_dumpall -p 9000 -g --gp-syntax > %s" % global_file_name
        output = self.dumper.create_pgdump_command_line()
        self.assertEquals(output, expected_output)

    @patch('gppylib.operations.dump.get_filter_file', return_value = '/tmp/update_test')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1'])
    @patch('gppylib.operations.dump.execute_sql', return_value = [('public','ao_part_table','ao_part_table_1_prt_p1'), ('public','ao_part_table','ao_part_table_1_prt_p2')])
    def test_update_filter_file_00(self, mock1, mock2, mock3):
        master_datadir = '/tmp'
        backup_dir = None
        dump_database = 'testdb'
        master_port = 5432
        filter_filename = '/tmp/update_test'

        update_filter_file(dump_database, master_datadir, backup_dir, master_port, self.dumper.dump_dir, self.dumper.dump_prefix)
        contents = get_lines_from_file(filter_filename)
        expected_result = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2']
        self.assertEqual(expected_result.sort(), contents.sort())
        os.remove(filter_filename)

    @patch('gppylib.operations.dump.get_filter_file', return_value = '/tmp/update_test')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2'])
    @patch('gppylib.operations.dump.execute_sql', return_value = [('public','ao_part_table','ao_part_table_1_prt_p1')])
    def test_update_filter_file_01(self, mock1, mock2, mock3):
        master_datadir = '/tmp'
        backup_dir = None
        dump_database = 'testdb'
        master_port = 5432
        filter_filename = '/tmp/update_test'

        update_filter_file(dump_database, master_datadir, backup_dir, master_port, self.dumper.dump_dir, self.dumper.dump_prefix)
        contents = get_lines_from_file(filter_filename)
        expected_result = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2']
        self.assertEqual(expected_result.sort(), contents.sort())
        os.remove(filter_filename)

    @patch('gppylib.operations.dump.get_filter_file', return_value='/tmp/update_test')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.heap_table1', 'public.ao_part_table', 'public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2'])
    @patch('gppylib.operations.dump.execute_sql', return_value=[('public', 'ao_part_table', 'ao_part_table_1_prt_p1')])
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_update_filter_file_02(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = '/tmp'
        backup_dir = None
        dump_database = 'testdb'
        master_port = 5432
        filter_filename = '/tmp/update_test'
        netbackup_service_host = "mdw"
        netbackup_policy = "nbu_policy"
        netbackup_schedule = "nbu_schedule"
        netbackup_block_size = "1024"

        update_filter_file(dump_database, master_datadir, backup_dir, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size)
        contents = get_lines_from_file(filter_filename)
        expected_result = ['public.heap_table1', 'public.ao_part_table', 'public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2']
        self.assertEqual(expected_result.sort(), contents.sort())
        os.remove(filter_filename)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_00(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_01(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_02(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_03(self, mock1):
        master_datadir = None
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_04(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_05(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_06(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_07(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_08(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_09(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_10(self, mock1):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_11(self, mock1):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_12(self, mock1):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_state_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, timestamp_key, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_06(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_09(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_10(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_11(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, timestamp_key, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_cdatabase_file_with_nbu_12(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_cdatabase_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_06(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_09(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_10(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_11(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_report_file_with_nbu_12(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_report_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None
        
        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)
        
    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_06(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_09(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_10(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_11(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_global_file_with_nbu_12(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/datadomain"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_global_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_00(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_01(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = None
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_02(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = None
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        netbackup_schedule = "test_schedule"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_03(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = None
        backup_dir = None
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_04(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        timestamp_key = "20140014000000"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_05(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = None
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        timestamp_key = "20140014000000"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_06(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = None
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        timestamp_key = "20140014000000"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_07(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = None
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_08(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = None
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None
        mock_segs = [Mock(), Mock()]
        timestamp_key = "20140014000000"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_09(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_10(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = None
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"
        mock_segs = [Mock(), Mock()]
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_11(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = "/data"
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"
        mock_segs = [Mock(), Mock()]
        timestamp_key = "20140014000000"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"
            seg.getSegmentHostName.return_value = "sdw"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_master_config_filename')
    @patch('gppylib.operations.dump.generate_segment_config_filename')
    @patch('gppylib.operations.dump.GpArray.initFromCatalog')
    @patch('gppylib.operations.dump.GpArray.getDbList')
    def test_backup_config_files_with_nbu_12(self, mock1, mock2, mock3, mock4, mock5):
        master_datadir = None
        backup_dir = "/datadomain"
        master_port = "5432"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = "foo"
        mock_segs = [Mock(), Mock()]
        timestamp_key = "20140014000000"
        mock_segs = [Mock(), Mock()]
        for id, seg in enumerate(mock_segs):
            seg.isSegmentPrimary.return_value = True
            seg.getSegmentDbId.return_value = id + 1
            seg.getSegmentDataDirectory.return_value = "/data"

        backup_config_files_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, master_port, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.generate_schema_filename', result='foo_schema')
    @patch('gppylib.operations.dump.copy_file_to_dd')
    def backup_schema_file_with_ddboost_00(self, mock1, mock2):
        master_datadir = '/data'
        backup_dir = None
        dump_dir = 'backup/DCA-35'
        backup_schema_file_with_ddboost(master_datadir, backup_dir, dump_dir, self.dumper.dump_prefix)

    @patch('gppylib.operations.dump.generate_report_filename', result='foo.rpt')
    @patch('gppylib.operations.dump.copy_file_to_dd')
    def test_backup_report_file_with_ddboost_00(self, mock1, mock2):
        master_datadir = '/data'
        backup_dir = None
        dump_dir = 'backup/DCA-35'
        backup_report_file_with_ddboost(master_datadir, backup_dir, dump_dir, self.dumper.dump_prefix)

    @patch('gppylib.operations.dump.generate_increments_filename', result='foo.increments')
    @patch('gppylib.operations.dump.copy_file_to_dd')
    def test_backup_increments_file_with_ddboost_00(self, mock1, mock2):
        master_datadir = '/data'
        backup_dir = None
        dump_dir = 'backup/DCA-35'

        backup_increments_file_with_ddboost(master_datadir, backup_dir, dump_dir, self.dumper.dump_prefix, full_timestamp='20140101')

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_dirtytable_filename')
    def test_backup_dirty_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_06(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_09(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_10(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_11(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_dirty_file_with_nbu_12(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_dirty_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_dirtytable_filename')
    def test_backup_increments_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_06(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_09(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_10(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_11(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_increments_file_with_nbu_12(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        full_timestamp = "20130101000000"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_increments_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, full_timestamp, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_dirtytable_filename')
    def test_backup_partition_list_file_with_nbu_00(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_01(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_02(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_03(self, mock1, mock2):
        master_datadir = None
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = None

        with self.assertRaisesRegexp(Exception, 'Master data directory and backup directory are both none.'):
            backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_04(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_05(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_06(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_07(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_08(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = None

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_09(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_10(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = None
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_11(self, mock1, mock2):
        master_datadir = "/data"
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = 100
        netbackup_keyword = "foo"

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    @patch('gppylib.operations.dump.generate_cdatabase_filename')
    def test_backup_partition_list_file_with_nbu_12(self, mock1, mock2):
        master_datadir = None
        backup_dir = "/tmp"
        netbackup_service_host = "mdw"
        netbackup_policy = "test_policy"
        netbackup_schedule = "test_schedule"
        timestamp_key = "20140014000000"
        netbackup_block_size = None
        netbackup_keyword = "foo"

        backup_partition_list_file_with_nbu(master_datadir, backup_dir, self.dumper.dump_dir, self.dumper.dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key)

    @patch('gppylib.operations.dump.execute_sql', return_value = [['gp_toolkit'], ['pg_toast'], ['pg_bitmapindex'], ['bar'], ['foo'], ['pg_catalog'], ['public'], ['information_schema']])
    def test_get_include_schema_list_from_exclude_schema_00(self, mock1):
        master_port = "5432"
        dbname = 'testdb'
        catalog_schema_list = ['gp_toolkit', 'information_schema']
        exclude_schema_list = ['public', 'foo']
        expected_result = ['bar']
        output = get_include_schema_list_from_exclude_schema(exclude_schema_list, catalog_schema_list, master_port, dbname)
        self.assertEqual(expected_result.sort(), output.sort())

    @patch('gppylib.operations.dump.execute_sql', return_value = [['gp_toolkit'], ['pg_toast'], ['pg_bitmapindex'], ['bar'], ['foo'], ['pg_catalog'], ['public'], ['information_schema']])
    def test_get_include_schema_list_from_exclude_schema_01(self, mock1):
        master_port = "5432"
        dbname = 'testdb'
        catalog_schema_list = ['gp_toolkit', 'information_schema']
        exclude_schema_list = []
        expected_result = ['public', 'foo', 'bar']
        output = get_include_schema_list_from_exclude_schema(exclude_schema_list, catalog_schema_list, master_port, dbname)
        self.assertEqual(expected_result.sort(), output.sort())

class MailEventTestCase(unittest.TestCase):

    def setUp(self):
        self.mailEvent = MailEvent(subject="test", message="Hello", to_addrs="example@gopivotal.com")

    @patch('gppylib.operations.dump.Command.run')
    @patch('gppylib.operations.dump.findCmdInPath', return_value='/bin/mail')
    def test_mail_execute_00(self, mock1, mock2):
        m = MailEvent(subject="test", message="Hello", to_addrs="example@gopivotal.com")
        m.execute()

if __name__ == '__main__':
    unittest.main()
