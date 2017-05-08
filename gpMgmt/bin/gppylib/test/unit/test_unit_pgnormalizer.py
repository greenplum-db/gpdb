#!/usr/bin/env python
# -*- coding: utf-8 -*-

import unittest
import sys
import StringIO
import mock

import pgnormalizer

__copyright__ = "Copyright (c) 2017-Present Pivotal Software, Inc"


class PGNOptionTest(unittest.TestCase):
    def test_option_empty(self):
        argv = [""]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_with_both_r_and_e(self):
        argv = ["", "-r", "-e"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_empty_without_r_or_e(self):
        argv = ["", "-c", "abc"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_column_delimiter_too_long(self):
        argv = ["", "-r", "-c", "123456789"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_line_delimiter_too_long(self):
        argv = ["", "-r", "-l", "123456789"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_column_delimiter_contains_tab(self):
        argv = ["", "-r", "-c", "123\t"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_column_delimiter_contains_lf(self):
        argv = ["", "-r", "-c", "123\n"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_line_delimiter_contains_tab(self):
        argv = ["", "-r", "-l", "123\t"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_line_delimiter_contains_lf(self):
        argv = ["", "-r", "-l", "123\n"]
        self.assertRaises(pgnormalizer.PGNError, pgnormalizer.PGNOption, argv)

    def test_option_with_r_and_c(self):
        argv = ["", "-r", "-c", "abc"]
        pgn = pgnormalizer.PGNOption(argv)
        self.assertEqual(pgn.column_delimiter, "abc")
        self.assertTrue(pgn.raw_mode)
        self.assertFalse(pgn.escape_mode)

    def test_option_with_r_and_l(self):
        argv = ["", "-r", "-l", "abc"]
        pgn = pgnormalizer.PGNOption(argv)
        self.assertEqual(pgn.line_delimiter, "abc")
        self.assertTrue(pgn.raw_mode)
        self.assertFalse(pgn.escape_mode)

    def test_option_with_r_and_lc(self):
        argv = ["", "-e", "-c", "abc", "-l", "def"]
        pgn = pgnormalizer.PGNOption(argv)
        self.assertEqual(pgn.column_delimiter, "abc")
        self.assertEqual(pgn.line_delimiter, "def")
        self.assertTrue(pgn.escape_mode)
        self.assertFalse(pgn.raw_mode)

    def test_option_unprintable_chars(self):
        argv = ["", "-r", "-c", "\\x080d", "-l", "\\x0a"]
        pgn = pgnormalizer.PGNOption(argv)
        self.assertEqual(pgn.column_delimiter, "\b\r")
        self.assertEqual(pgn.line_delimiter, "\n")
        self.assertTrue(pgn.raw_mode)
        self.assertFalse(pgn.escape_mode)

    def test_option_escape_chars(self):
        argv = ["", "-r", "-c", "\\b\\r", "-l", "\\b\\b"]
        pgn = pgnormalizer.PGNOption(argv)
        self.assertEqual(pgn.column_delimiter, "\b\r")
        self.assertEqual(pgn.line_delimiter, "\b\b")
        self.assertTrue(pgn.raw_mode)
        self.assertFalse(pgn.escape_mode)


class PGNReaderTest(unittest.TestCase):
    def test_write_reading_from_line(self):
        lines = "1abc11abc111\n2abc22abc222\n3abc33abc333"
        argv = ["", "-r", "-c", "abc"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t11\t111\n2\t22\t222\n3"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])

    def test_write_reading_from_chuck(self):
        lines = "1abc11abc111def2abc22abc222def3abc33abc333"
        argv = ["", "-r", "-c", "abc", "-l", "def"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t11\t111\n2\t22\t222\n3"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])

    def test_write_reading_from_chuck_raw(self):
        lines = "1abc1\n1abc11\t1def2abc2\\t2abc222def3\\nabc33abc333"
        argv = ["", "-r", "-c", "abc", "-l", "def"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t1\\n1\t11\\t1\n2\t2\\\\t2\t222\n3\\\\n"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])

    def test_write_reading_from_chuck_escape(self):
        lines = "1abc1\n1abc11\t1def2abc2\\t2abc222def3\\nabc33abc333"
        argv = ["", "-e", "-c", "abc", "-l", "def"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t1\\n1\t11\\t1\n2\t2\\t2\t222\n3\\n"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])

    def test_write_reading_from_small_chuck(self):
        lines = "1abc11abc111def2abc22abc222def3abc33abc333"
        argv = ["", "-r", "-c", "abc", "-l", "def"]

        in_source = StringIO.StringIO(lines)
        out_source = StringIO.StringIO()
        options = pgnormalizer.PGNOption(argv)
        pgn = pgnormalizer.PGNormalizer(8, in_source, out_source)
        pgn.write_out(options)
        self.assertEqual("1\t11\t111\n2\t22\t222\n3\t33\t333", out_source.getvalue())

    def test_write_with_single_unprintable_char(self):
        lines = "1\b11\b111\v2\b22\b222\v3\b33\b333"
        argv = ["", "-r", "-c", "\\x08", "-l", "\\x0b"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t11\t111\n2\t22\t222\n3"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])

    def test_write_with_multiple_unprintable_chars(self):
        lines = "1\b\f11\b\f111\v2\b\f22\b\f222\v3\b\f33\b\f333"
        argv = ["", "-r", "-c", "\\x080c", "-l", "\\x0b"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t11\t111\n2\t22\t222\n3"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])

    def test_write_with_multiple_unprintable_chars_lc(self):
        lines = "1\b\f11\b\f111\b\v2\b\f22\b\f222\b\v3\b\f33\b\f333"
        argv = ["", "-r", "-c", "\\x080c", "-l", "\\x080b"]

        in_source = StringIO.StringIO(lines)
        options = pgnormalizer.PGNOption(argv)
        with mock.patch('sys.stdout') as fake_stdout:
            pgn = pgnormalizer.PGNormalizer(1024, in_source, sys.stdout)
            pgn.write_out(options)

        fake_stdout.assert_has_calls([
            mock.call.write("1\t11\t111\n2\t22\t222\n3"),
            mock.call.write('\t33\t333'),
            mock.call.flush()
        ])


if __name__ == '__main__':
    unittest.main()
