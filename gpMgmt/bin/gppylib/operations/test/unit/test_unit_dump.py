#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import unittest
from datetime import datetime
from gppylib.commands.base import Command, CommandResult
from gppylib.gparray import GpArray, Segment
from gppylib.operations.backup_utils import *
from gppylib.operations.dump import *
from mock import patch, MagicMock, Mock, mock_open, call, ANY
from . import setup_fake_gparray

class DumpTestCase(unittest.TestCase):

    @patch('gppylib.operations.backup_utils.Context.get_master_port', return_value = 5432)
    def setUp(self, mock1):
        with patch('gppylib.gparray.GpArray.initFromCatalog', return_value=setup_fake_gparray()):
            context = Context()
            context.target_db ='testdb'
            context.dump_schema='testschema'
            context.include_dump_tables_file='/tmp/table_list.txt'
            context.master_datadir=context.backup_dir='/data/master'
            context.batch_default=None
            context.timestamp_key = '20160101010101'
            context.generate_dump_timestamp()
            context.schema_file = None

        self.context = context
        self.dumper = DumpDatabase(self.context)
        self.dump_globals = DumpGlobal(self.context)
        self.mailEvent = MailEvent(subject="test", message="Hello", to_addrs="example@pivotal.io")

    @patch('gppylib.operations.dump.get_heap_partition_list', return_value=[['123', 'public', 't4'], ['123', 'public', 't5'], ['123', 'testschema', 't6']])
    def test_get_dirty_heap_tables_default(self, mock1):
        expected_output = set(['public.t4', 'public.t5', 'testschema.t6'])
        dirty_table_list = get_dirty_heap_tables(self.context)
        self.assertEqual(dirty_table_list, expected_output)

    @patch('gppylib.operations.dump.get_heap_partition_list', return_value=[[], ['123', 'public', 't5'], ['123', 'public', 't6']])
    def test_get_dirty_heap_tables_empty_arg(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Heap tables query returned rows with unexpected number of columns 0'):
            dirty_table_list = get_dirty_heap_tables(self.context)

    def test_write_dirty_file_default(self):
        dirty_tables = ['t1', 't2', 't3']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            tmpfilename = write_dirty_file(self.context, dirty_tables)
            result = m()
            self.assertEqual(len(dirty_tables), len(result.write.call_args_list))
            for i in range(len(dirty_tables)):
                self.assertEqual(call(dirty_tables[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='test_dirty_filename')
    def test_write_dirty_file_timestamp(self, mock1):
        dirty_tables = ['t1', 't2', 't3']
        timestamp = '20160101010101'
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            tmpfilename = write_dirty_file(self.context, dirty_tables, timestamp)
            mock1.assert_called_with("dirty_table", timestamp=timestamp)
            result = m()
            self.assertEqual(len(dirty_tables), len(result.write.call_args_list))
            for i in range(len(dirty_tables)):
                self.assertEqual(call(dirty_tables[i]+'\n'), result.write.call_args_list[i])

            result = m()
            self.assertEqual(len(dirty_tables), len(result.write.call_args_list))
            for i in range(len(dirty_tables)):
                self.assertEqual(call(dirty_tables[i]+'\n'), result.write.call_args_list[i])

    def test_write_dirty_file_no_list(self):
        dirty_tables = None
        tmpfilename = write_dirty_file(self.context, dirty_tables)
        self.assertEqual(tmpfilename, None)

    def test_write_dirty_file_empty_list(self):
        dirty_tables = []
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            tmpfilename = write_dirty_file(self.context, dirty_tables)
            result = m()
            self.assertEqual(len(result.write.call_args_list), 0)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20120330120102', '20120330120103'])
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', return_value='20120330120102')
    def test_validate_increments_file_default(self, mock1, mock2):
        # expect no exception to die out of this
        CreateIncrementsFile.validate_increments_file(self.context, '/tmp/fn')

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20120330120102', '20120330120103'])
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', side_effect=Exception('invalid timestamp'))
    def test_validate_increments_file_bad_increment(self, mock1, mock2):

        with self.assertRaisesRegexp(Exception, "Timestamp '20120330120102' from increments file '/tmp/fn' is not a valid increment"):
            CreateIncrementsFile.validate_increments_file(self.context, '/tmp/fn')

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20120330120102', '20120330120103'])
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', return_value=None)
    def test_validate_increments_file_empty_file(self, mock1, mock2):

        with self.assertRaisesRegexp(Exception, "Timestamp '20120330120102' from increments file '/tmp/fn' is not a valid increment"):
            CreateIncrementsFile.validate_increments_file(self.context, '/tmp/fn')

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test_CreateIncrementsFile_init(self, mock1, mock2, mock3):
        obj = CreateIncrementsFile(self.context)
        self.assertEquals(obj.increments_filename, '/data/master/db_dumps/20160101/gp_dump_20160101000000_increments')

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    @patch('gppylib.operations.dump.get_lines_from_file', side_effect=[ [], ['20160101010101'] ])
    def test_CreateIncrementsFile_execute_no_file(self, mock1, mock2, mock3, mock4):
        obj = CreateIncrementsFile(self.context)
        with patch('__builtin__.open', mock_open(), create=True):
            result = obj.execute()
            self.assertEquals(1, result)

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.get_incremental_ts_from_report_file', return_value='')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20160101010101'])
    def test_CreateIncrementsFile_execute_invalid_timestamp(self, mock1, mock2, mock3, mock4):
        obj = CreateIncrementsFile(self.context)
        with self.assertRaisesRegexp(Exception, ".* is not a valid increment"):
            obj.execute()

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.get_lines_from_file', side_effect=[ ['20160101010000'], ['20160101010000', '20160101010101'] ])
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test_CreateIncrementsFile_execute_append(self, mock1, mock2, mock3, mock4):
        obj = CreateIncrementsFile(self.context)
        with patch('__builtin__.open', mock_open(), create=True):
            result = obj.execute()
            self.assertEquals(2, result)

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test_CreateIncrementsFile_execute_no_output(self, mock1, mock2, mock3, mock4):
        obj = CreateIncrementsFile(self.context)
        with patch('__builtin__.open', mock_open(), create=True):
            with self.assertRaisesRegexp(Exception, 'File not written to'):
                result = obj.execute()

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20160101000000'])
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test_CreateIncrementsFile_execute_wrong_timestamp(self, mock1, mock2, mock3, mock4):
        obj = CreateIncrementsFile(self.context)
        with patch('__builtin__.open', mock_open(), create=True):
            with self.assertRaisesRegexp(Exception, 'Timestamp .* not written to'):
                result = obj.execute()

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.get_lines_from_file', side_effect=[ ['20160101010000'], ['20160101010001', '20160101010101'] ])
    @patch('gppylib.operations.dump.CreateIncrementsFile.validate_increments_file')
    def test_CreateIncrementsFile_execute_modified_timestamp(self, mock1, mock2, mock3, mock4):
        obj = CreateIncrementsFile(self.context)
        with patch('__builtin__.open', mock_open(), create=True):
            with self.assertRaisesRegexp(Exception, 'trouble adding timestamp'):
                result = obj.execute()

    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_write_partition_list_file_no_filter_file(self, mock1):
        with patch('gppylib.operations.dump.get_partition_list') as p:
            part_list = [[123, 'myschema', 't1'], [4444, 'otherschema', 't2'], [992313, 'public', 't3']]
            p.return_value = part_list
            m = mock_open()
            with patch('__builtin__.open', m, create=True):
                write_partition_list_file(self.context)
                result = m()
                self.assertEqual(len(part_list), len(result.write.call_args_list))
                for i in range(len(part_list)):
                    expected = "%s.%s\n" % (part_list[i][1], part_list[i][2])
                    self.assertEqual(call(expected), result.write.call_args_list[i])

    @patch('gppylib.operations.dump.get_partition_list', return_value=[['t1', 'foo', 'koo'], ['public', 't2'], ['public', 't3']])
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_write_partition_list_file_bad_query_return(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Invalid results from query to get all tables'):
            write_partition_list_file(self.context)

    def test_create_dump_outcome_default(self):
        start = datetime(2012, 7, 31, 9, 30, 00)
        end = datetime(2012, 8, 1, 12, 21, 11)
        rc = 5
        expected_outcome = {'timestamp_start': '20120731093000',
                            'time_start': '09:30:00',
                            'time_end': '12:21:11',
                            'exit_status': 5}
        outcome = self.dumper.create_dump_outcome(start, end, rc)
        self.assertTrue(expected_outcome == outcome)

    @patch('gppylib.operations.dump.ValidateDumpDatabase.run')
    @patch('gppylib.operations.dump.Command.run')
    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.dump.DumpDatabase.create_filter_file')
    def test_execute_default(self, mock1, mock2, mock3, mock4):
        self.context.include_dump_tables_file = ''
        self.dumper.execute()
        # should not raise any exceptions

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.execSQLForSingleton', return_value='100')
    def test_get_partition_state_default(self, mock1, mock2, mock3):
        partition_info = [(123, 'testschema', 't1', 4444), (234, 'testschema', 't2', 5555)]
        expected_output = ['testschema, t1, 100', 'testschema, t2, 100']
        result = get_partition_state(self.context, 'pg_aoseg', partition_info)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    def test_get_partition_state_empty(self, mock1, mock2):
        partition_info = []
        expected_output = []
        result = get_partition_state(self.context, 'pg_aoseg', partition_info)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.execSQLForSingleton', return_value='10000000000000000')
    def test_get_partition_state_exceeded_count(self, mock1, mock2, mock3):
        partition_info = [(123, 'testschema', 't1', 4444), (234, 'testschema', 't2', 5555)]
        expected_output = ['testschema, t1, 10000000000000000', 'testschema, t2, 10000000000000000']
        with self.assertRaisesRegexp(Exception, 'Exceeded backup max tuple count of 1 quadrillion rows per table for:'):
            get_partition_state(self.context, 'pg_aoseg', partition_info)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.execSQLForSingleton', return_value='100')
    def test_get_partition_state_many_partition(self, mock1, mock2, mock3):
        master_port=5432
        dbname='testdb'
        partition_info = [(123, 'testschema', 't1', 4444), (234, 'testschema', 't2', 5555)] * 1
        expected_output = ['testschema, t1, 100', 'testschema, t2, 100'] * 1
        result = get_partition_state(self.context, 'pg_aoseg', partition_info)
        self.assertEqual(result, expected_output)

    def test_get_filename_from_filetype_ao(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_20160101010101_ao_state_file'
        result = get_filename_from_filetype(self.context, "ao", self.context.timestamp)
        self.assertEqual(result, expected_output)

    def test_get_filename_from_filetype_co(self):
        expected_output = '/data/master/db_dumps/20160101/gp_dump_20160101010101_co_state_file'
        result = get_filename_from_filetype(self.context, "co", self.context.timestamp)
        self.assertEqual(result, expected_output)

    def test_get_filename_from_filetype_bad_type(self):
        with self.assertRaisesRegexp(Exception, 'Invalid table type *'):
            result = get_filename_from_filetype(self.context, "schema", self.context.timestamp)

    def test_write_state_file_bad_type(self):
        table_type = 'foo'
        partition_list = ['testschema, t1, 100', 'testschema, t2, 100']
        with self.assertRaisesRegexp(Exception, 'Invalid table type *'):
            write_state_file(self.context, table_type, partition_list)

    @patch('gppylib.operations.dump.get_filename_from_filetype', return_value='/tmp/db_dumps/20160101/gp_dump_20160101010101')
    def test_write_state_file_default(self, mock1):
        table_type = 'ao'
        part_list = ['testschema, t1, 100', 'testschema, t2, 100']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            write_state_file(self.context, table_type, part_list)
            result = m()
            self.assertEqual(len(part_list), len(result.write.call_args_list))
            for i in range(len(part_list)):
                self.assertEqual(call(part_list[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.dump.get_filename_from_filetype', return_value='/tmp/db_dumps/20170413/gp_dump_20170413224743_ao_state_file')
    def test_write_state_file_empty(self, mock1):
        table_type = 'ao'
        part_list = ['']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            write_state_file(self.context, table_type, part_list)
            result = m()
            self.assertEqual(1, len(result.write.call_args_list))
            for i in range(len(part_list)):
                self.assertEqual(call('\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.dump.execute_sql', return_value=[['public', 'ao_table', 123, 'CREATE', 'table', '2012: 1'], ['testschema', 'co_table', 333, 'TRUNCATE', '', '2033 :1 - 111']])
    def test_get_last_operation_data_default(self, mock):
        output = get_last_operation_data(self.context)
        expected = ['public,ao_table,123,CREATE,table,2012: 1', 'testschema,co_table,333,TRUNCATE,,2033 :1 - 111']
        self.assertEquals(output, expected)

    @patch('gppylib.operations.dump.execute_sql', return_value=[])
    def test_get_last_operation_data_empty(self, mock):
        output = get_last_operation_data(self.context)
        expected = []
        self.assertEquals(output, expected)

    @patch('gppylib.operations.dump.execute_sql', return_value=[[123, 'table', '2012: 1'], [333, 'TRUNCATE', '', '2033 :1 - 111']])
    def test_get_last_operation_data_invalid(self, mock):
        with self.assertRaisesRegexp(Exception, 'Invalid return from query'):
            get_last_operation_data(self.context)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['testschema, t1, 100', 'testschema, t2, 100'])
    def test_get_last_state_default(self, mock1, mock2, mock3):
        table_type = 'ao'
        expected_output = ['testschema, t1, 100', 'testschema, t2, 100']
        output = get_last_state(self.context, table_type)
        self.assertEqual(output, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=False)
    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo')
    def test_get_last_state_no_file(self, mock1, mock2, mock3):
        table_type = 'ao'
        with self.assertRaisesRegexp(Exception, 'ao state file does not exist: foo'):
            get_last_state(self.context, table_type)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    def test_get_last_state_empty_file(self, mock1, mock2, mock3):
        table_type = 'ao'
        output = get_last_state(self.context, table_type)
        self.assertEqual(output, [])

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101121212')
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=True)
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    def test_get_last_state_nbu(self, mock1, mock2, mock3, mock4, mock5):
        table_type = 'ao'
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = "1024"
        output = get_last_state(self.context, table_type)
        self.assertEqual(output, [])

    def test_compare_dict_different(self):
        last_dict = {'testschema.t1':'100', 'testschema.t2':'200'}
        curr_dict = {'testschema.t1':'200', 'testschema.t2':'200'}
        expected_output = set(['testschema.t1'])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_compare_dict_extra(self):
        last_dict = {'testschema.t1':'100', 'testschema.t2':'200', 'testschema.t3':'300'}
        curr_dict = {'testschema.t1':'100', 'testschema.t2':'100'}
        expected_output = set(['testschema.t2'])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_compare_dict_missing(self):
        last_dict = {'testschema.t1':'100', 'testschema.t2':'200'}
        curr_dict = {'testschema.t1':'100', 'testschema.t2':'200', 'testschema.t3':'300'}
        expected_output = set(['testschema.t3'])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_compare_dict_identical(self):
        last_dict = {'testschema.t1':'100', 'testschema.t2':'200'}
        curr_dict = {'testschema.t1':'100', 'testschema.t2':'200'}
        expected_output = set([])
        result = compare_dict(last_dict, curr_dict)
        self.assertEqual(result, expected_output)

    def test_create_partition_dict_default(self):
        partition_list = ['testschema, t1, 100', 'testschema, t2, 200']
        expected_output = {'testschema.t1':'100', 'testschema.t2':'200'}
        result = create_partition_dict(partition_list)
        self.assertEqual(result, expected_output)

    def test_create_partition_dict_empty(self):
        partition_list = ['']
        expected_output = {}
        result = create_partition_dict(partition_list)
        self.assertEqual(result, expected_output)

    def test_create_partition_dict_invalid_format(self):
        partition_list = ['testschema t1 100']
        with self.assertRaisesRegexp(Exception, 'Invalid state file format *'):
            create_partition_dict(partition_list)

    @patch('gppylib.operations.backup_utils.Context.generate_filename')
    @patch('gppylib.operations.dump.os.path.isdir', return_value=False)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=False)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_last_dump_timestamp_default(self, mock1, mock2, mock3, mock4):
        full_timestamp = '20160101000000'
        result = get_last_dump_timestamp(self.context)
        self.assertEqual(result, full_timestamp)

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['20160101010000', '20160101010001'])
    @patch('gppylib.operations.dump.os.path.isdir', return_value=True)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_last_dump_timestamp_one_previous(self, mock1, mock2, mock3, mock4):
        master_datadir = 'foo'
        backup_dir = None
        full_timestamp = '20160101000000'
        expected_output = '20160101010001'
        result = get_last_dump_timestamp(self.context)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.os.path.isdir', return_value=True)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['2012093009300q'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_last_dump_timestamp_invalid_timestamp(self, mock1, mock2, mock3, mock4):
        with self.assertRaisesRegexp(Exception, 'get_last_dump_timestamp found invalid ts in file'):
            get_last_dump_timestamp(self.context)

    @patch('gppylib.operations.dump.os.path.isdir', return_value=True)
    @patch('gppylib.operations.dump.os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[' 20160101010101 \n  \n  '])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_last_dump_timestamp_extra_whitespace(self, mock1, mock2, mock3, mock4):
        expected = '20160101010101'
        result = get_last_dump_timestamp(self.context)
        self.assertEqual(result, expected)

    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=False)
    def test_get_last_dump_timestamp_nbu(self, mock1, mock2):
        netbackup_service_host = "mdw"
        netbackup_block_size = "1024"
        expected = '20160101000000'
        result = get_last_dump_timestamp(self.context)
        self.assertEqual(result, expected)

    def test_get_pgstatlastoperations_dict_single_input(self):
        last_operations = ['public,t1,1234,ALTER,,201601011212:101010']
        last_operations_dict = get_pgstatlastoperations_dict(last_operations)
        expected_output = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010'}
        self.assertEqual(last_operations_dict, expected_output)

    def test_get_pgstatlastoperations_dict_multiple_input(self):
        last_operations = ['public,t1,1234,ALTER,,201601011212:101010', 'public,t2,1234,VACCUM,TRUNCATE,201601011212:101015']
        last_operations_dict = get_pgstatlastoperations_dict(last_operations)
        expected_output = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010',
                           ('1234', 'VACCUM'): 'public,t2,1234,VACCUM,TRUNCATE,201601011212:101015'}
        self.assertEqual(last_operations_dict, expected_output)

    def test_get_pgstatlastoperations_dict_empty(self):
        last_operations = ['']
        last_operations_dict = get_pgstatlastoperations_dict(last_operations)
        expected_output = {}
        self.assertEqual(last_operations_dict, expected_output)

    def test_get_pgstatlastoperations_dict_invalid_input(self):
        last_operations = ['public,t1,1234,ALTER,,201601011212:101010',  '2345,VACCUM,TRUNCATE,201601011212:101015']
        with self.assertRaisesRegexp(Exception, 'Wrong number of tokens in last_operation data for last backup'):
            get_pgstatlastoperations_dict(last_operations)

    def test_compare_metadata_(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201601011212:101010']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set())

    def test_compare_metadata_different_keyword(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010'}
        cur_metadata = ['public,t1,1234,TRUNCATE,,201601011212:101010']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set(['public.t1']))

    def test_compare_metadata_different_timestamp(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201601011212:102510']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set(['public.t1']))

    def test_compare_metadata_duplicate_input(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201601011212:101010','public,t1,1234,TRUNCATE,,201601011212:101010']
        dirty_tables = compare_metadata(old_metadata, cur_metadata)
        self.assertEquals(dirty_tables, set(['public.t1']))

    def test_compare_metadata_invalid_input(self):
        old_metadata = {('1234', 'ALTER'): 'public,t1,1234,ALTER,,201601011212:101010'}
        cur_metadata = ['public,t1,1234,ALTER,,201601011212:101010,']
        with self.assertRaisesRegexp(Exception, 'Wrong number of tokens in last_operation data for current backup'):
            compare_metadata(old_metadata, cur_metadata)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=[])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_tables_with_dirty_metadata_empty(self, mock1, mock2, mock3):
        expected_output = set()
        full_timestamp = '20160101010101'
        cur_pgstatoperations = []
        dirty_tables = get_tables_with_dirty_metadata(self.context, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_tables_with_dirty_metadata_default(self, mock1, mock2, mock3):
        expected_output = set()
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510']
        dirty_tables = get_tables_with_dirty_metadata(self.context, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102511'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_tables_with_dirty_metadata_changed_table(self, mock1, mock2, mock3):
        expected_output = set(['testschema.t2'])
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510']
        dirty_tables = get_tables_with_dirty_metadata(self.context, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['testschema,t1,2234,TRUNCATE,,201601011213:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_tables_with_dirty_metadata_extras(self, mock1, mock2, mock3):
        expected_output = set(['testschema.t2', 'public.t3'])
        full_timestamp = '20160101010101'
        cur_pgstatoperations = ['testschema,t2,1234,ALTER,CHANGE COLUMN,201601011212:102510',
                                'testschema,t2,2234,TRUNCATE,,201601011213:102510',
                                'public,t3,2234,TRUNCATE,,201601011213:102510']
        dirty_tables = get_tables_with_dirty_metadata(self.context, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)


    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['testschema,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101000000')
    def test_get_tables_with_dirty_metadata_different_schema(self, mock1, mock2, mock3):
        expected_output = set(['public.t1'])
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510']
        dirty_tables = get_tables_with_dirty_metadata(self.context, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_dump_timestamp', return_value='20160101010100')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['testschema,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510'])
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    def test_get_tables_with_dirty_metadata_nbu(self, mock1, mock2, mock3):
        expected_output = set(['public.t1'])
        cur_pgstatoperations = ['public,t1,1234,ALTER,CHANGE COLUMN,201601011212:102510', 'testschema,t2,2234,TRUNCATE,,201601011213:102510']
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = "1024"
        dirty_tables = get_tables_with_dirty_metadata(self.context, cur_pgstatoperations)
        self.assertEqual(dirty_tables, expected_output)

    @patch('gppylib.operations.dump.get_last_state', return_value=['testschema, t1, 100', 'testschema, t2, 200'])
    def test_get_dirty_partition_tables_default(self, mock1):
        table_type = 'ao'
        curr_state_partition_list = ['testschema, t3, 300', 'testschema, t1, 200']
        expected_output = set(['testschema.t3', 'testschema.t1'])
        result = get_dirty_partition_tables(self.context, table_type, curr_state_partition_list)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.get_last_state', return_value=['testschema, t1, 100', 'testschema, t2, 200'])
    def test_get_dirty_partition_tables_nbu(self, mock1):
        table_type = 'ao'
        curr_state_partition_list = ['testschema, t3, 300', 'testschema, t1, 200']
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = "1024"
        expected_output = set(['testschema.t3', 'testschema.t1'])
        result = get_dirty_partition_tables(self.context, table_type, curr_state_partition_list)
        self.assertEqual(result, expected_output)

    @patch('gppylib.operations.dump.get_dirty_heap_tables', return_value=set(['public.heap_table1']))
    @patch('gppylib.operations.dump.get_dirty_partition_tables', side_effect=[set(['public,ao_t1,100', 'public,ao_t2,100']), set(['public,co_t1,100', 'public,co_t2,100'])])
    @patch('gppylib.operations.dump.get_tables_with_dirty_metadata', return_value=set(['public,ao_t3,1234,CREATE,,20160101101010', 'public,co_t3,2345,VACCUM,,20160101101010', 'public,ao_t1,1234,CREATE,,20160101101010']))
    def test_get_dirty_tables(self, mock1, mock2, mock3):
        ao_partition_list = []
        co_partition_list = []
        last_operation_data = []
        dirty_tables = get_dirty_tables(self.context, ao_partition_list, co_partition_list, last_operation_data)
        expected_output = ['public.heap_table1', 'public.ao_t1', 'public.ao_t2', 'public.co_t1', 'public.co_t2', 'public.ao_t3', 'public.co_t3']
        self.assertEqual(dirty_tables.sort(), expected_output.sort())

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20160101010100')
    def test_validate_current_timestamp_default(self, mock):
        directory = '/foo'
        #no exception
        validate_current_timestamp(self.context, current='20160101010101')

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20160101010101')
    def test_validate_current_timestamp_same_timestamp(self, mock):
        directory = '/foo'
        with self.assertRaisesRegexp(Exception, 'There is a future dated backup on the system preventing new backups'):
            validate_current_timestamp(self.context, current='20160101010101')

    @patch('gppylib.operations.dump.get_latest_report_timestamp', return_value = '20170101010101')
    def test_validate_current_timestamp_future_timestamp(self, mock):
        directory = '/foo'
        with self.assertRaisesRegexp(Exception, 'There is a future dated backup on the system preventing new backups'):
            validate_current_timestamp(self.context, current='20160101010101')

    def test_validate_modcount_default(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '999999999999999'
        validate_modcount(schemaname, partitionname, tuple_count)

    def test_validate_modcount_non_int(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '#########'
        with self.assertRaisesRegexp(Exception, 'Can not convert modification count for table.'):
            validate_modcount(schemaname, partitionname, tuple_count)

    def test_validate_modcount_scientific_notation(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '1+e15'
        with self.assertRaisesRegexp(Exception, 'Can not convert modification count for table.'):
            validate_modcount(schemaname, partitionname, tuple_count)

    def test_validate_modcount_exceeded_count(self):
        schemaname = 'public'
        partitionname = 't1'
        tuple_count = '1000000000000000'
        with self.assertRaisesRegexp(Exception, 'Exceeded backup max tuple count of 1 quadrillion rows per table for:'):
            validate_modcount(schemaname, partitionname, tuple_count)

    def test_generate_dump_timestamp_default(self):
        ts_key = datetime(2013, 02, 04, 10, 10, 10, 10000).strftime("%Y%m%d%H%M%S")
        self.context.timestamp_key = ts_key
        self.context.generate_dump_timestamp()
        self.assertEqual(ts_key, self.context.timestamp)
        self.assertEqual(ts_key[0:8], self.context.db_date_dir)

    def test_generate_dump_timestamp_no_timestamp(self):
        self.context.timestamp_key = None
        self.context.generate_dump_timestamp()
        self.assertNotEqual(None, self.context.timestamp)
        self.assertNotEqual(None, self.context.db_date_dir)

    def test_generate_dump_timestamp_replace_timestamp(self):
        ts1 = datetime(2013, 02, 04, 10, 10, 10, 10000)
        ts2 = datetime(2013, 03, 04, 10, 10, 10, 10000)
        self.context.timestamp_key = ts1.strftime("%Y%m%d%H%M%S")
        self.context.generate_dump_timestamp()
        self.context.timestamp_key = ts2.strftime("%Y%m%d%H%M%S")
        self.context.generate_dump_timestamp()
        ts_key = ts2.strftime("%Y%m%d%H%M%S")
        self.assertEqual(ts_key, self.context.timestamp)
        self.assertEqual(ts_key[0:8], self.context.db_date_dir)

    def test_create_dump_string_with_prefix_schema_level_dump(self):
        self.context.dump_prefix = 'foo_'
        self.context.schema_file = '/tmp/schema_file '
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" "testdb" --schema-file=/tmp/schema_file """
            self.assertEquals(output, expected_output)

    def test_create_dump_string_default(self):
        self.context.schema_file = '/tmp/schema_file'
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --no-expand-children -n "\\"testschema\\"" "testdb" --schema-file=/tmp/schema_file"""
            self.assertEquals(output, expected_output)

    def test_create_dump_string_without_incremental(self):
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --no-expand-children -n "\\"testschema\\"" "testdb" --table-file=/tmp/table_list.txt"""
            self.assertEquals(output, expected_output)

    def test_create_dump_string_with_prefix(self):
        self.context.dump_prefix = 'foo_'
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" "testdb" --table-file=/tmp/table_list.txt"""
            self.assertEquals(output, expected_output)

    def test_create_dump_string_with_include_file(self):
        self.context.dump_prefix = 'metro_'
        self.context.include_dump_tables_file = 'bar'
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --prefix=metro_ --no-expand-children -n "\\"testschema\\"" "testdb" --table-file=%s""" % self.context.include_dump_tables_file
            self.assertEquals(output, expected_output)

    def test_create_dump_string_with_no_file_args(self):
        self.context.dump_prefix = 'metro_'
        self.context.include_dump_tables_file = None
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --prefix=metro_ --no-expand-children -n "\\"testschema\\"" "testdb\""""
            self.assertEquals(output, expected_output)

    def test_create_dump_string_with_netbackup_params(self):
        self.context.include_dump_tables_file = None
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --no-expand-children -n "\\"testschema\\"" "testdb" --netbackup-service-host=mdw --netbackup-policy=test_policy --netbackup-schedule=test_schedule"""
            self.assertEquals(output, expected_output)

    def test_get_backup_dir_with_master_data_dir(self):
        self.assertEquals('/data/master/db_dumps/20160101', self.context.get_backup_dir())

    def test_get_backup_dir_with_backup_dir(self):
        self.context.backup_dir = '/tmp'
        self.assertEquals('/tmp/db_dumps/20160101', self.context.get_backup_dir())

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101010101')
    @patch('os.path.isfile', return_value=True)
    def test_get_filter_file_file_exists(self, mock1, mock2, mock3):
        self.context.dump_prefix = 'foo_'
        expected_output = '/data/master/db_dumps/20160101/foo_gp_dump_20160101010101_filter'
        self.assertEquals(expected_output, get_filter_file(self.context))

    @patch('os.path.isfile', return_value=False)
    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101010101')
    @patch('gppylib.operations.dump.get_latest_full_ts_with_nbu', return_value='20160101010101')
    @patch('gppylib.operations.dump.check_file_dumped_with_nbu', return_value=True)
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    def test_get_filter_file_file_exists_on_nbu(self, mock1, mock2, mock3, mock4, mock5, mock6):
        self.context.dump_prefix = 'foo_'
        self.context.netbackup_block_size = "1024"
        self.context.netbackup_service_host = "mdw"
        expected_output = '/data/master/db_dumps/20160101/foo_gp_dump_20160101010101_filter'
        self.assertEquals(expected_output, get_filter_file(self.context))

    @patch('gppylib.operations.backup_utils.Context.is_timestamp_in_old_format', return_value=False)
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20160101010101')
    @patch('os.path.isfile', return_value=False)
    def test_get_filter_file_file_does_not_exist(self, mock1, mock2, mock3):
        self.assertEquals(None, get_filter_file(self.context))

    def test_update_filter_file_with_dirty_list_default(self):
        filter_file = '/tmp/foo'
        dirty_tables = ['public.t1', 'public.t2']
        expected_output = ['public.t1', 'public.t2']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            update_filter_file_with_dirty_list(filter_file, dirty_tables)
            result = m()
            self.assertEqual(len(dirty_tables), len(result.write.call_args_list))
            for i in range(len(dirty_tables)):
                self.assertEqual(call(dirty_tables[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.backup_utils.get_lines_from_file', return_value=['public.t1', 'public.t2'])
    def test_update_filter_file_with_dirty_list_duplicates(self, mock1):
        filter_file = '/tmp/foo'
        dirty_tables = ['public.t2']
        expected_output = ['public.t1', 'public.t2']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            update_filter_file_with_dirty_list(filter_file, dirty_tables)
            result = m()
            self.assertEqual(len(dirty_tables), len(result.write.call_args_list))
            for i in range(len(dirty_tables)):
                self.assertEqual(call(dirty_tables[i]+'\n'), result.write.call_args_list[i])

    def test_update_filter_file_with_dirty_list_empty_file(self):
        filter_file = '/tmp/foo'
        dirty_tables = ['public.t1', 'public.t2']
        expected_output = ['public.t1', 'public.t2']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            update_filter_file_with_dirty_list(filter_file, dirty_tables)
            result = m()
            self.assertEqual(len(dirty_tables), len(result.write.call_args_list))
            for i in range(len(dirty_tables)):
                self.assertEqual(call(dirty_tables[i]+'\n'), result.write.call_args_list[i])

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'testschema.t2'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    @patch('gppylib.operations.dump.get_filter_file', return_value='/foo/metro_gp_dump_20130101010101_filter')
    @patch('gppylib.operations.dump.get_latest_full_ts_with_nbu', return_value='20130101010101')
    def test_filter_dirty_tables_with_filter(self, mock1, mock2, mock3, mock4):
        dirty_tables = ['public.t1', 'public.t2', 'testschema.t1', 'testschema.t2']
        expected_output = ['public.t1', 'testschema.t2']
        self.context.netbackup_service_host = 'mdw'
        self.assertEquals(sorted(expected_output), sorted(filter_dirty_tables(self.context, dirty_tables)))

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'testschema.t2'])
    @patch('gppylib.operations.dump.get_filter_file', return_value='/foo/metro_gp_dump_20130101010101_filter')
    @patch('gppylib.operations.dump.get_latest_full_ts_with_nbu', return_value='20130101010101')
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    def test_filter_dirty_tables_with_filter_with_nbu(self, mock1, mock2, mock3, mock4):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_block_size = "1024"
        dirty_tables = ['public.t1', 'public.t2', 'testschema.t1', 'testschema.t2']
        expected_output = ['public.t1', 'testschema.t2']
        self.assertEquals(sorted(expected_output), sorted(filter_dirty_tables(self.context, dirty_tables)))

    @patch('gppylib.operations.dump.get_lines_from_file', return_value=['public.t1', 'testschema.t2'])
    @patch('gppylib.operations.dump.get_latest_full_dump_timestamp', return_value='20130101010101')
    @patch('gppylib.operations.dump.get_filter_file', return_value=None)
    def test_filter_dirty_tables_without_filter(self, mock1, mock2, mock3):
        dirty_tables = ['public.t1', 'public.t2', 'testschema.t1', 'testschema.t2']
        self.assertEquals(sorted(dirty_tables), sorted(filter_dirty_tables(self.context, dirty_tables)))

    @patch('gppylib.operations.dump.get_filter_file', return_value='/tmp/db_dumps/20160101/foo_gp_dump_01234567891234_filter')
    def test_create_filtered_dump_string(self, mock1):
        self.context.dump_prefix = 'foo_'
        with patch.dict(os.environ, {'LOGNAME':'gpadmin'}):
            output = self.dumper.create_filtered_dump_string()
            expected_output = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=20160101010101 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" "testdb" --table-file=/tmp/table_list.txt --incremental-filter=/tmp/db_dumps/20160101/foo_gp_dump_01234567891234_filter"""
            self.assertEquals(output, expected_output)

    @patch('gppylib.operations.dump.Command.get_results', return_value=CommandResult(0, "", "", True, False))
    @patch('gppylib.operations.dump.Command.run')
    def test_perform_dump_normal(self, mock1, mock2):
        self.context.dump_prefix = 'foo_'
        title = 'Dump process'
        dump_line = """gp_dump -p 5432 -U gpadmin --gp-d=/data/master/db_dumps/20160101 --gp-r=/data/master/db_dumps/20160101 --gp-s=p --gp-k=01234567891234 --no-lock --gp-c --prefix=foo_ --no-expand-children -n "\\"testschema\\"" "testdb" --table-file=/tmp/table_list.txt"""
        (start, end, rc) = self.dumper.perform_dump(title, dump_line)
        self.assertNotEqual(start, None)
        self.assertNotEqual(end, None)
        self.assertEquals(rc, 0)

    def test_create_pgdump_command_line(self):
        global_file_name = '/data/master/db_dumps/20160101/gp_global_-1_1_20160101010101'
        expected_output = "pg_dumpall -p 5432 -g --gp-syntax > %s" % global_file_name
        output = self.dump_globals.create_pgdump_command_line()
        self.assertEquals(output, expected_output)

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_filter_file', return_value = '/tmp/update_test')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1'])
    @patch('gppylib.operations.dump.execute_sql', side_effect = [ [['public.ao_part_table']], [['public.ao_part_table_1_prt_p1'], ['public.ao_part_table_1_prt_p2']] ])
    def test_update_filter_file_default(self, mock1, mock2, mock3, mock4):
        filter_filename = '/tmp/update_test'
        contents = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1']
        expected_result = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            update_filter_file(self.context)
            result = m()
            self.assertEqual(len(expected_result), len(result.write.call_args_list))
            expected = sorted(expected_result)
            output = sorted(result.write.call_args_list)
            for i in range(len(expected)):
                self.assertEqual(call(expected[i]+'\n'), output[i])

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.dump.get_filter_file', return_value = '/tmp/update_test')
    @patch('gppylib.operations.dump.get_lines_from_file', return_value = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1'])
    @patch('gppylib.operations.dump.execute_sql', side_effect = [ [['public.ao_part_table']], [['public.ao_part_table_1_prt_p1'], ['public.ao_part_table_1_prt_p2']] ])
    @patch('gppylib.operations.dump.restore_file_with_nbu')
    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_update_filter_file_default_with_nbu(self, mock1, mock2, mock3, mock4, mock5, mock6):
        filter_filename = '/tmp/update_test'
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "nbu_policy"
        self.context.netbackup_schedule = "nbu_schedule"
        self.context.netbackup_block_size = "1024"
        contents = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1']
        expected_result = ['public.heap_table1','public.ao_part_table','public.ao_part_table_1_prt_p1', 'public.ao_part_table_1_prt_p2']
        m = mock_open()
        with patch('__builtin__.open', m, create=True):
            update_filter_file(self.context)
            result = m()
            self.assertEqual(len(expected_result), len(result.write.call_args_list))
            expected = sorted(expected_result)
            output = sorted(result.write.call_args_list)
            for i in range(len(expected)):
                self.assertEqual(call(expected[i]+'\n'), output[i])

    @patch('gppylib.operations.dump.backup_file_with_nbu')
    def test_backup_state_files_with_nbu_default(self, mock):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"

        backup_state_files_with_nbu(self.context)
        self.assertEqual(mock.call_count, 3)

    class MyMock(MagicMock):
        def __init__(self, num_segs):
            super(MagicMock, self).__init__()
            self.mock_segs = []
            for i in range(num_segs):
                self.mock_segs.append(Mock())

        def getSegmentList(self):
            for id, seg in enumerate(self.mock_segs):
                seg.get_active_primary.getSegmentHostName.return_value = Mock()
                seg.get_primary_dbid.return_value = id + 2
            return self.mock_segs

    @patch('gppylib.gparray.Segment.getSegmentHostName', return_value='sdw')
    def test_backup_config_files_with_nbu_default(self, mock1):
        with patch('gppylib.operations.dump.backup_file_with_nbu', side_effect=my_counter) as nbu_mock:
            global i
            i = 0
            self.context.netbackup_service_host = "mdw"
            self.context.netbackup_policy = "test_policy"
            self.context.netbackup_schedule = "test_schedule"

            backup_config_files_with_nbu(self.context)
            args, _ = nbu_mock.call_args_list[0]
            self.assertEqual(args[1], "master_config")
            for id, seg in enumerate(mock1.mock_segs):
                self.assertEqual(seg.get_active_primary.call_count, 1)
                self.assertEqual(seg.get_primary_dbid.call_count, 1)
                args, _ = nbu_mock.call_args_list[id]
                self.assertEqual(args, ("segment_config", id+2, "sdw"))
            self.assertEqual(i, 3)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_ddboost_default(self, mock1, mock2):
        self.context.backup_dir = None
        self.context.dump_dir = 'backup/DCA-35'
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_ddboost(self.context, "schema")
            cmd.assert_called_with("copy file foo_schema to DD machine", "gpddboost --copyToDDBoost --from-file=foo_schema --to-file=backup/DCA-35/20160101/foo_schema")
            self.assertEqual(mock2.call_count, 1)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_ddboost_no_filetype(self, mock1, mock2):
        self.context.backup_dir = None
        self.context.dump_dir = 'backup/DCA-35'
        with self.assertRaisesRegexp(Exception, 'Cannot call backup_file_with_ddboost without a filetype argument'):
            backup_file_with_ddboost(self.context)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_default(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        self.context.netbackup_block_size = 100
        cmdStr = "cat /tmp/foo_schema | gp_bsa_dump_agent --netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule --netbackup-filename /tmp/foo_schema --netbackup-block-size 100"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("dumping metadata files from master", cmdStr)
            self.assertEqual(mock2.call_count, 1)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_no_filetype(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        self.context.netbackup_block_size = 100
        cmdStr = "cat /tmp/foo_schema | gp_bsa_dump_agent --netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule --netbackup-filename /tmp/foo_schema --netbackup-block-size 100"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_nbu(self.context, path="/tmp/foo_schema")
            cmd.assert_called_with("dumping metadata files from master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_no_path(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        self.context.netbackup_block_size = 100
        cmdStr = "cat /tmp/foo_schema | gp_bsa_dump_agent --netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule --netbackup-filename /tmp/foo_schema --netbackup-block-size 100"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("dumping metadata files from master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_both_args(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Cannot supply both a file type and a file path to backup_file_with_nbu'):
            backup_file_with_nbu(self.context, "schema", "/tmp/foo_schema")

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_neither_arg(self, mock1, mock2):
        with self.assertRaisesRegexp(Exception, 'Cannot call backup_file_with_nbu with no type or path argument'):
            backup_file_with_nbu(self.context)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_block_size(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        self.context.netbackup_block_size = 1024
        cmdStr = "cat /tmp/foo_schema | gp_bsa_dump_agent --netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule --netbackup-filename /tmp/foo_schema --netbackup-block-size 1024"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("dumping metadata files from master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_keyword(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        self.context.netbackup_block_size = 100
        self.context.netbackup_keyword = "foo"
        cmdStr = "cat /tmp/foo_schema | gp_bsa_dump_agent --netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule --netbackup-filename /tmp/foo_schema --netbackup-block-size 100 --netbackup-keyword foo"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_nbu(self.context, "schema")
            cmd.assert_called_with("dumping metadata files from master", cmdStr)

    @patch('gppylib.operations.backup_utils.Context.generate_filename', return_value='/tmp/foo_schema')
    @patch('gppylib.commands.base.Command.run')
    def test_backup_file_with_nbu_segment(self, mock1, mock2):
        self.context.netbackup_service_host = "mdw"
        self.context.netbackup_policy = "test_policy"
        self.context.netbackup_schedule = "test_schedule"
        self.context.netbackup_block_size = 100
        cmdStr = "cat /tmp/foo_schema | gp_bsa_dump_agent --netbackup-service-host mdw --netbackup-policy test_policy --netbackup-schedule test_schedule --netbackup-filename /tmp/foo_schema --netbackup-block-size 100"
        with patch.object(Command, '__init__', return_value=None) as cmd:
            backup_file_with_nbu(self.context, "schema", hostname="sdw")
            from gppylib.commands.base import REMOTE
            cmd.assert_called_with("dumping metadata files from segment", cmdStr, ctxt=REMOTE, remoteHost="sdw")

    @patch('gppylib.operations.dump.execute_sql', return_value = [['gp_toolkit'], ['pg_aoseg'], ['pg_toast'], ['pg_bitmapindex'], ['bar'], ['foo'], ['pg_catalog'], ['public'], ['information_schema']])
    def test_get_include_schema_list_from_exclude_schema_default(self, mock1):
        exclude_schema_list = ['public', 'foo']
        expected_result = ['bar']
        output = get_include_schema_list_from_exclude_schema(self.context, exclude_schema_list)
        self.assertEqual(expected_result.sort(), output.sort())

    @patch('gppylib.operations.dump.execute_sql', return_value = [['gp_toolkit'], ['pg_aoseg'], ['pg_toast'], ['pg_bitmapindex'], ['bar'], ['foo'], ['pg_catalog'], ['public'], ['information_schema']])
    def test_get_include_schema_list_from_exclude_schema_empty_list(self, mock1):
        exclude_schema_list = []
        expected_result = ['public', 'foo', 'bar']
        output = get_include_schema_list_from_exclude_schema(self.context, exclude_schema_list)
        self.assertEqual(expected_result.sort(), output.sort())

    @patch('gppylib.operations.dump.Command.run')
    @patch('gppylib.operations.dump.findCmdInPath', return_value='/bin/mail')
    def test_mail_execute_default(self, mock1, mock2):
        m = MailEvent(subject="test", message="Hello", to_addrs="example@pivotal.io")
        m.execute()

    @patch('gppylib.operations.dump.execute_sql', side_effect=[[['public', 'test'], ['public', 'foo']], [['public', 'foo']]])
    def test_check_table_exists_table_list_changes(self, mock):
        self.context.target_db = "gptest"
        exists = CheckTableExists(self.context, "public", "test").run()
        self.assertTrue(exists)
        exists = CheckTableExists(self.context, "public", "test").run()
        self.assertFalse(exists)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.CheckTableExists.run', return_value=True)
    @patch('gppylib.operations.dump.execSQL', return_value='10000000000000000')
    def test_update_history_table_with_existing_history_table(self, execSQL_mock, mock2, mock3, mock4):
        self.context.history = True
        time_start = datetime(2015, 7, 31, 9, 30, 00)
        time_end = datetime(2015, 8, 1, 12, 21, 11)
        timestamp = '121601010101'
        options_list = '-x 1337 -a'
        dump_exit_status = 0
        pseudo_exit_status = 0
        UpdateHistoryTable(self.context, time_start, time_end,
                           options_list, timestamp,
                           dump_exit_status,
                           pseudo_exit_status).execute()

        expected_queries = " insert into public.gpcrondump_history values (now(), '2015-07-31 09:30:00', '2015-08-01 12:21:11', '-x 1337 -a', '121601010101', 0, 0, 'COMPLETED'); "
        for exec_sql in execSQL_mock.call_args_list:
            # [0] index removes the call object,
            # [1] grabs the sql command from execSQL
            self.assertEquals(exec_sql[0][1], expected_queries)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    @patch('gppylib.operations.dump.CheckTableExists.run', return_value=False)
    @patch('gppylib.operations.dump.execSQL', return_value='10000000000000000')
    def test_update_history_table_with_new_update_table(self, execSQL_mock, mock2, mock3, mock4):
        self.context.history = True
        time_start = datetime(2015, 7, 31, 9, 30, 00)
        time_end = datetime(2015, 8, 1, 12, 21, 11)
        timestamp = '121601010101'
        options_list = '-x bkdb -a'
        dump_exit_status = 0
        pseudo_exit_status = 0
        UpdateHistoryTable(self.context, time_start, time_end,
                           options_list, timestamp,
                           dump_exit_status,
                           pseudo_exit_status).execute()

        expected_queries = []
        expected_queries.append(' create table public.gpcrondump_history (rec_date timestamp, start_time char(8), end_time char(8), options text, dump_key varchar(20), dump_exit_status smallint, script_exit_status smallint, exit_text varchar(10)) distributed by (rec_date); ')
        expected_queries.append(" insert into public.gpcrondump_history values (now(), '2015-07-31 09:30:00', '2015-08-01 12:21:11', '-x bkdb -a', '121601010101', 0, 0, 'COMPLETED'); ")
        for i, exec_sql in enumerate(execSQL_mock.call_args_list):
            # [0] index removes the call object,
            # [1] grabs the sql command from execSQL
            self.assertEquals(exec_sql[0][1] , expected_queries[i])

    @patch('gppylib.operations.dump.DumpStats.print_tuples')
    @patch('gppylib.operations.dump.execute_sql_with_connection', return_value=[[1]*4, [2]*4, [3]*4])
    def test_dump_stats_writes_tuples_to_file_when_dumping_tuples(self, execute_sql_with_connection, print_tuples):
        dump_stats = DumpStats(Mock())

        db_connection = Mock()
        dump_stats.dump_tuples('select * from foo', db_connection)

        execute_sql_with_connection.assert_called_with('select * from foo', db_connection)
        print_tuples.assert_any_call([1,1,1,1])
        print_tuples.assert_any_call([2,2,2,2])
        print_tuples.assert_any_call([3,3,3,3])

    @patch('gppylib.operations.dump.DumpStats.print_stats')
    @patch('gppylib.operations.dump.execute_sql_with_connection', return_value=[[1]*25, [2]*25, [3]*25])
    def test_dump_stats_writes_stats_to_file_when_dumping_stats(self, execute_sql_with_connection, print_stats):
        dump_stats = DumpStats(Mock())

        db_connection = Mock()
        dump_stats.dump_stats('select * from foo', db_connection)

        execute_sql_with_connection.assert_called_with('select * from foo', db_connection)
        print_stats.assert_any_call([1]*25)
        print_stats.assert_any_call([2]*25)
        print_stats.assert_any_call([3]*25)

    @patch('gppylib.operations.dump.DumpStats.dump_tuples')
    @patch('gppylib.operations.dump.DumpStats.dump_stats')
    def test_dump_stats_uses_db_connection_to_dump_tables(self, dump_stats, dump_tuples):
        db_connection = Mock()

        subject = DumpStats(Mock())
        subject.dump_table('someSchema.someTable', db_connection)

        dump_stats.assert_called_with(ANY, db_connection)
        dump_tuples.assert_called_with(ANY, db_connection)

    @patch('gppylib.operations.dump.dbconn.DbURL')
    @patch('gppylib.operations.dump.dbconn.connect')
    def test_excute_uses_the_same_connection_for_all_queries(self, connect, DbURL):
        DbURL.return_value = 'dburl'

        db_connection = Mock()
        connect.return_value = db_connection

        fakeContext = Mock()
        fakeContext.ddboost = False
        fakeContext.master_port = 9999
        fakeContext.target_db= 'db_name'

        dump_stats = DumpStats(fakeContext)
        dump_stats.get_include_tables_from_context = Mock(return_value=['schema1.table1', 'schema2.table2'])
        dump_stats.write_stats_file_header = Mock()
        dump_stats.dump_table = Mock()

        dump_stats.execute()

        dump_stats.dump_table.assert_any_call('schema1.table1', db_connection)
        dump_stats.dump_table.assert_any_call('schema2.table2', db_connection)

        connect.assert_called_with('dburl')
        DbURL.assert_called_with(port=9999, dbname='db_name')

        db_connection.close.assert_any_call()

if __name__ == '__main__':
    unittest.main()

i=0
def my_counter(*args, **kwargs):
    global i
    i += 1
    return Mock()
