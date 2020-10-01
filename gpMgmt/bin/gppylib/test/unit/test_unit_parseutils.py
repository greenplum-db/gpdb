import sys, os
from mock import Mock, patch
from gppylib.test.unit.gp_unittest import GpTestCase
from gppylib.parseutils import is_valid_datadir, is_valid_port, is_valid_contentid, is_valid_dbid,\
    is_valid_role, is_valid_address, line_reader, check_values
from gppylib.mainUtils import ExceptionNoStackTraceNeeded

class GpParseUtilsTest(GpTestCase):
    def setUp(self):
        super(GpParseUtilsTest, self).setUp()

    def tearDown(self):
        super(GpParseUtilsTest, self).tearDown()

    def test_is_valid_port(self):
        self.assertTrue(is_valid_port('0'))
        self.assertTrue(is_valid_port('2'))
        self.assertTrue(is_valid_port('12345'))
        self.assertFalse(is_valid_port('chocolate'))
        self.assertFalse(is_valid_port(''))
        self.assertFalse(is_valid_port('-1'))

    def test_is_valid_contentid(self):
        self.assertTrue(is_valid_contentid('-1'))
        self.assertTrue(is_valid_contentid('2'))
        self.assertTrue(is_valid_contentid('1000'))
        self.assertFalse(is_valid_contentid('-2'))
        self.assertFalse(is_valid_contentid('chocolate'))
        self.assertFalse(is_valid_contentid(''))

    def test_is_valid_dbid(self):
        self.assertTrue(is_valid_dbid('1'))
        self.assertTrue(is_valid_dbid('2'))
        self.assertTrue(is_valid_dbid('200'))
        self.assertFalse(is_valid_dbid('-3'))
        self.assertFalse(is_valid_dbid('chocolate'))
        self.assertFalse(is_valid_dbid(''))
        self.assertFalse(is_valid_dbid('-1'))
        self.assertFalse(is_valid_dbid('0'))

    def test_is_valid_role(self):
        self.assertTrue(is_valid_role('p'))
        self.assertTrue(is_valid_role('m'))
        self.assertFalse(is_valid_role('x'))
        self.assertFalse(is_valid_role(''))

    def test_is_valid_address(self):
        self.assertTrue(is_valid_address('127.0.0.1'))
        self.assertTrue(is_valid_address('pivotal.io'))
        self.assertTrue(is_valid_address('bluegreenred'))
        self.assertTrue(is_valid_address('::1'))
        self.assertFalse(is_valid_address(''))

    def test_is_valid_datadir(self):
        self.assertTrue(is_valid_datadir('foobar'))
        self.assertFalse(is_valid_datadir(''))

    def test_line_reader(self):
        mylist = ['', '# test', 'abc:def']
        gen = line_reader(mylist)
        offset, line = next(gen)
        self.assertTrue(offset == 3 and line == 'abc:def')
        with self.assertRaises(StopIteration):
            next(gen)

        l1 = "first"
        l2 = "second"
        gen = line_reader([l1,l2])
        offset, line = next(gen)
        self.assertTrue(offset == 1 and line == l1)
        offset, line = next(gen)
        self.assertTrue(offset == 2 and line == l2)
        with self.assertRaises(StopIteration):
            next(gen)

    def test_check_values_negative(self):
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, address='')
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, port='not_a_port')
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, datadir='')
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, content='not_a_content_id')
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, hostname='')
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, dbid='not_a_dbid')
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            check_values(1, role='x')

