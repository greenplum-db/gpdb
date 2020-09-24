#!/usr/bin/env python3

import os
import unittest

from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations.reload import GpReload
from mock import MagicMock, Mock, mock_open, patch

class GpReloadTestCase(unittest.TestCase):

    def setUp(self):
        class Options:
            def __init__(self):
                if 'PGPORT' not in os.environ:
                    self.port = None
                else:
                    self.port = os.environ['PGPORT']

                self.table_file = '/tmp/table_file'
                self.database = 'testdb'
                self.interactive = False
        args = None
        self.reload = GpReload(Options(), args)

    def tearDown(self):
        if self.reload.table_file and os.path.exists(self.reload.table_file):
            os.remove(self.reload.table_file)

    def test_validate_options_table_file_not_specified(self):
        self.reload.table_file = None
        with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, 'Please specify table file'):
            self.reload.validate_options()

    @patch('os.path.exists', return_value=False)
    def test_validate_options_table_file_does_not_exist(self, mock1):
        with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, 'Unable to find table file "/tmp/table_file"'):
            self.reload.validate_options()

    @patch('os.path.exists', return_value=True)
    def test_validate_options_database_not_specified(self, mock1):
        self.reload.database = None
        with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, 'Please specify the correct database'):
            self.reload.validate_options()

    @patch('os.path.exists', return_value=True)
    @patch('os.environ')
    def test_validate_options_no_port(self, mock1, mock2):
        self.reload.port = None
        with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, 'Please specify PGPORT using -p option or set PGPORT in the environment'):
            self.reload.validate_options()

    @patch('os.path.exists', return_value=True)
    def test_validate_options_valid(self, mock1):
        self.reload.validate_options()

    def test_parse_line_empty_table(self):
        line = 'public.:sort_column'
        with self.assertRaises(Exception):
            self.reload.parse_line(line)

    def test_parse_line_empty_schema(self):
        line = '.t1:sort_column'
        with self.assertRaises(Exception):
            self.reload.parse_line(line)

    def test_parse_line_empty_columns(self):
        line = 'public.t1:'
        with self.assertRaises(Exception):
            self.reload.parse_line(line)

    def test_parse_columns_empty_line(self):
        columns = ''
        with self.assertRaisesRegex(Exception, 'Empty column'):
            self.reload.parse_columns(columns)

    def test_parse_columns_single_column(self):
        columns = 'single_col'
        self.assertEqual([('single_col', 'asc')], self.reload.parse_columns(columns))

    def test_parse_columns_empty_columns(self):
        columns = '   , c2 foo'
        with self.assertRaisesRegex(Exception, 'Empty column'):
            self.reload.parse_columns(columns)

    def test_parse_column_single_sort_order(self):
        columns = 'single_col asc'
        self.assertEqual([('single_col', 'asc')], self.reload.parse_columns(columns))

    def test_parse_column_invalid_sort_order1(self):
        columns = 'c1, c2 foo'
        with self.assertRaisesRegex(Exception, 'Invalid sort order foo'):
            self.reload.parse_columns(columns)

    def test_parse_column_invalid_sort_order2(self):
        columns = 'c1, c2 foo bar'
        with self.assertRaisesRegex(Exception, 'Invalid sort order specified'):
            self.reload.parse_columns(columns)

    def test_validate_table_file_empty_table_name(self):
        self.reload.table_file = '/tmp/table_file'

        with open(self.reload.table_file, 'w') as fp:
            fp.write('public.t1: a, b\n')
            fp.write('public.: a, b\n')

        try:
            with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, "Line 'public.: a, b' is not formatted correctly"):
                self.reload.validate_table_file()
        finally:
            if os.path.exists(self.reload.table_file):
                os.remove(self.reload.table_file)

    def test_validate_table_file_empty_schema_name(self):
        self.reload.table_file = '/tmp/table_file'

        with open(self.reload.table_file, 'w') as fp:
            fp.write('public.t1: a, b\n')
            fp.write('.t1: a, b\n')

        try:
            with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, "Line '.t1: a, b' is not formatted correctly"):
                self.reload.validate_table_file()
        finally:
            if os.path.exists(self.reload.table_file):
                os.remove(self.reload.table_file)

    def test_validate_table_file_empty_column_list(self):
        self.reload.table_file = '/tmp/table_file'

        with open(self.reload.table_file, 'w') as fp:
            fp.write('public.t1:\n')

        try:
            with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, "Line 'public.t1:' is not formatted correctly"):
                self.reload.validate_table_file()
        finally:
            if os.path.exists(self.reload.table_file):
                os.remove(self.reload.table_file)

    def test_validate_table_file_invalid_sort_order(self):
        self.reload.table_file = '/tmp/table_file'

        with open(self.reload.table_file, 'w') as fp:
            fp.write('public.t1: a, b\n')
            fp.write('public.t2: a, b foo\n')

        try:
            with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, "Line 'public.t2: a, b foo' is not formatted correctly"):
                self.reload.validate_table_file()
        finally:
            if os.path.exists(self.reload.table_file):
                os.remove(self.reload.table_file)

    def test_validate_table_file_valid_data1(self):
        self.reload.table_file = '/tmp/table_file'

        with open(self.reload.table_file, 'w') as fp:
            fp.write('public.t1: a, b\n')
            fp.write('public.t2: a, b\n')

        try:
            expected_output = [('public', 't1', [('a', 'asc'), ('b', 'asc')]), ('public', 't2', [('a', 'asc'), ('b', 'asc')])]
            self.assertEqual(expected_output, self.reload.validate_table_file())
        finally:
            if os.path.exists(self.reload.table_file):
                os.remove(self.reload.table_file)

    def test_validate_table_file_valid_data2(self):
        self.reload.table_file = '/tmp/table_file'

        with open(self.reload.table_file, 'w') as fp:
            fp.write('public.t1: a, b desc\n')
            fp.write('public.t2: a desc, b\n')

        try:
            expected_output = [('public', 't1', [('a', 'asc'), ('b', 'desc')]), ('public', 't2', [('a', 'desc'), ('b', 'asc')])]
            self.assertEqual(expected_output, self.reload.validate_table_file())
        finally:
            if os.path.exists(self.reload.table_file):
                os.remove(self.reload.table_file)

    @patch('gppylib.operations.reload.dbconn.connect')
    @patch('gppylib.operations.reload.dbconn.querySingleton', return_value=0)
    def test_validate_table_invalid_table(self, mock1, mock2):
        with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, "Table public.t1 does not exist"):
            self.reload.validate_table('public', 't1')

    @patch('gppylib.operations.reload.dbconn.connect')
    @patch('gppylib.operations.reload.dbconn.querySingleton', return_value=1)
    def test_validate_table_valid_table(self, mock1, mock2):
        self.reload.validate_table('public', 't1')

    @patch('gppylib.operations.reload.dbconn.connect')
    def test_validate_columns_invalid_column(self, mock1):
        m = Mock()
        m.fetchall.return_value=[['a', 'x'], ['b', 'y']]
        with patch('gppylib.operations.reload.dbconn.query', return_value=m) as p:
            with self.assertRaisesRegex(ExceptionNoStackTraceNeeded, 'Table public.t1 does not have column c'):
                self.reload.validate_columns('public', 't1', [('a', 'x'), ('b', 'y'), ('c', 'z')])

    @patch('gppylib.operations.reload.dbconn.connect')
    def test_validate_columns_valid_column1(self, mock1):
        m = Mock()
        m.fetchall.return_value=[['a', 'x'], ['b', 'y'], ['c', 'z']]
        with patch('gppylib.operations.reload.dbconn.query', return_value=m) as p:
            self.reload.validate_columns('public', 't1', [('a', 'x'), ('b', 'y'), ('c', 'z')])

    @patch('gppylib.operations.reload.dbconn.connect')
    def test_validate_columns_valid_column2(self, mock1):
        m = Mock()
        m.fetchall.return_value=[['a', 'w'], ['b', 'x'], ['c', 'y'], ['d', 'z']]
        with patch('gppylib.operations.reload.dbconn.query', return_value=m) as p:
            self.reload.validate_columns('public', 't1', [('a', 'w'), ('b', 'x'), ('c', 'y'), ('d', 'z')])

    @patch('gppylib.operations.reload.dbconn.querySingleton', return_value=0)
    @patch('gppylib.operations.reload.dbconn.connect')
    def test_check_indexes_no_indexes(self, mock1, mock2):
        self.assertTrue(self.reload.check_indexes('public', 't1'))

    @patch('gppylib.operations.reload.ask_yesno', return_value=True)
    @patch('gppylib.operations.reload.dbconn.connect')
    @patch('gppylib.operations.reload.dbconn.querySingleton', return_value=1)
    def test_check_indexes_with_indexes_with_continue(self, mock1, mock2, mock3):
        self.reload.interactive=True
        self.assertTrue(self.reload.check_indexes('public', 't1'))

    @patch('gppylib.operations.reload.ask_yesno', return_value=False)
    @patch('gppylib.operations.reload.dbconn.connect')
    @patch('gppylib.operations.reload.dbconn.querySingleton', return_value=1)
    def test_check_indexes_with_indexes_without_continue(self, mock1, mock2, mock3):
        self.reload.interactive=True
        self.assertFalse(self.reload.check_indexes('public', 't1'))

    @patch('gppylib.operations.reload.dbconn.connect')
    @patch('gppylib.operations.reload.dbconn.query', side_effect=[[('public', 'x1')], [('public', 'x2')]])
    def test_get_parent_partition_map(self, mock1, mock2):
        expected = {('public', 't1'): ('public', 'x1'), ('public', 't2'): ('public', 'x2')}
        self.reload.table_list = [('public', 't1', 'a b c d'), ('public', 't2', 'd e f g')]
        self.assertEqual(expected, self.reload.get_parent_partitions())

    @patch('gppylib.operations.reload.dbconn.connect') 
    @patch('gppylib.operations.reload.dbconn.query', side_effect=[[('public', 'x1')], []])
    def test_get_parent_partition_map_non_partition(self, mock1, mock2):
        expected = {('public', 't1'): ('public', 'x1'), ('public', 't2'): ('public', 't2')}
        self.reload.table_list = [('public', 't1', 'a b c d'), ('public', 't2', 'd e f g')]
        self.assertEqual(expected, self.reload.get_parent_partitions())
