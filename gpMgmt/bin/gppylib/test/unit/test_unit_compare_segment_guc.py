from mock import *
from .gp_unittest import *
from gpconfig_modules.compare_segment_guc import MultiValueGuc
from gpconfig_modules.database_segment_guc import DatabaseSegmentGuc
from gpconfig_modules.file_segment_guc import FileSegmentGuc


class CompareSegmentGucTest(GpTestCase):
    def setUp(self):
        row = ['contentid', 'guc_name', 'file_value', "dbid"]
        self.file_seg_guc = FileSegmentGuc(row)
        row = ['contentid', 'guc_name', 'sql_value']
        self.db_seg_guc = DatabaseSegmentGuc(row)

        self.subject = MultiValueGuc(self.file_seg_guc, self.db_seg_guc)

    def test_init_when_comparison_guc_supplied(self):
        row = ['contentid', 'guc_name', 'file_value', "diff_dbid"]
        file_seg_guc = FileSegmentGuc(row)
        old = self.subject

        self.subject = MultiValueGuc(self.subject, file_seg_guc)

        self.assertEqual(self.subject.db_seg_guc, old.db_seg_guc)
        self.assertEqual(self.subject.primary_file_seg_guc, old.primary_file_seg_guc)
        self.assertEqual(self.subject.mirror_file_seg_guc, file_seg_guc)

    def test_init_with_wrong_content_id_raises(self):
        row = ['contentid', 'guc_name', 'file_value', "dbid"]
        file_seg_guc = FileSegmentGuc(row)
        row = ['different', 'guc_name', 'sql_value']
        db_seg_guc = DatabaseSegmentGuc(row)

        with self.assertRaisesRegex(Exception, "Not the same context"):
            MultiValueGuc(file_seg_guc, db_seg_guc)

    def test_init_handles_both_orders(self):
        self.assertEqual(self.file_seg_guc, self.subject.primary_file_seg_guc)
        self.assertEqual(self.db_seg_guc, self.subject.db_seg_guc)
        self.assertTrue(isinstance(self.subject.primary_file_seg_guc, FileSegmentGuc))
        self.assertTrue(isinstance(self.subject.db_seg_guc, DatabaseSegmentGuc))

        self.subject = MultiValueGuc(self.db_seg_guc, self.file_seg_guc)

        self.assertEqual(self.file_seg_guc, self.subject.primary_file_seg_guc)
        self.assertEqual(self.db_seg_guc, self.subject.db_seg_guc)
        self.assertTrue(isinstance(self.subject.primary_file_seg_guc, FileSegmentGuc))
        self.assertTrue(isinstance(self.subject.db_seg_guc, DatabaseSegmentGuc))

    def test_init_when_none_raises(self):
        with self.assertRaisesRegex(Exception, "comparison requires two gucs"):
            self.subject = MultiValueGuc(self.db_seg_guc, None)

        with self.assertRaisesRegex(Exception, "comparison requires two gucs"):
            self.subject = MultiValueGuc(None, self.db_seg_guc)

    def test_report_fail_format_for_database_and_file_gucs(self):
        self.assertEqual(self.subject.report_fail_format(),
                          ["[context: contentid] [dbid: dbid] [name: guc_name] [value: sql_value | file: file_value]"])

    def test_report_fail_format_file_segment_guc_only(self):
        self.subject.db_seg_guc = None
        row = ['contentid', 'guc_name', 'primary_value', "dbid1"]
        self.subject.set_primary_file_segment(FileSegmentGuc(row))
        row = ['contentid', 'guc_name', 'mirror_value', "dbid2"]
        self.subject.set_mirror_file_segment(FileSegmentGuc(row))
        self.assertEqual(self.subject.report_fail_format(),
                          ["[context: contentid] [dbid: dbid1] [name: guc_name] [value: primary_value]",
                          "[context: contentid] [dbid: dbid2] [name: guc_name] [value: mirror_value]"])

    def test_when_segment_report_success_format(self):
        self.assertEqual(self.subject.report_success_format(),
                          "Segment     value: sql_value | file: file_value")

    def test_when_values_match_report_success_format_file_compare(self):
        self.subject.db_seg_guc.value = 'value'
        self.subject.primary_file_seg_guc.value = 'value'
        self.assertEqual(self.subject.report_success_format(), "Segment     value: value | file: value")

    def test_is_internally_consistent_fails(self):
        self.assertEqual(self.subject.is_internally_consistent(), False)

    def test_is_internally_consistent_when_file_value_is_none_succeeds(self):
        self.file_seg_guc.value = None
        self.assertEqual(self.subject.is_internally_consistent(), True)

    def test_is_internally_consistent_when_primary_is_same_succeeds(self):
        self.subject.primary_file_seg_guc.value = "sql_value"
        self.assertEqual(self.subject.is_internally_consistent(), True)

    def test_is_internally_consistent_when_mirror_is_different_fails(self):
        self.subject.primary_file_seg_guc.value = "sql_value"
        row = ['contentid', 'guc_name', 'diffvalue', "dbid1"]
        self.subject.set_mirror_file_segment(FileSegmentGuc(row))
        self.assertEqual(self.subject.is_internally_consistent(), False)

    def test_is_internally_consistent_with_quotes_and_escaping(self):
        cases = [
            {'file_value': "'value'", 'db_value': 'value'},
            {'file_value': "''", 'db_value': ''},
            {'file_value': "'\\n\\r\\b\\f\\t'", 'db_value': '\n\r\b\f\t'},
            {'file_value': "'\\0\\1\\2\\3\\4\\5\\6\\7'", 'db_value': '\0\1\2\3\4\5\6\7'},
            {'file_value': "'\\8'", 'db_value': '8'},
            {'file_value': "'\\01\\001\\377\\777\\7777'", 'db_value': '\x01\x01\xFF\xFF\xFF7'},
        ]
        for case in cases:
            file_seg_guc = FileSegmentGuc(['contentid', 'guc_name', case['file_value'], "dbid"])
            db_seg_guc = DatabaseSegmentGuc(['contentid', 'guc_name', case['db_value']])

            subject = MultiValueGuc(file_seg_guc, db_seg_guc)
            error_message = "expected file value: %r to be equal to db value: %r" % (case['file_value'], case['db_value'])
            self.assertEqual(subject.is_internally_consistent(), True, error_message)

    def test_is_internally_consistent_when_there_is_no_quoting(self):
        cases = [
            {'file_value': "value123", 'db_value': 'value123'},
            {'file_value': "value-._:/", 'db_value': 'value-._:/'},
        ]
        for case in cases:
            file_seg_guc = FileSegmentGuc(['contentid', 'guc_name', case['file_value'], "dbid"])
            db_seg_guc = DatabaseSegmentGuc(['contentid', 'guc_name', case['db_value']])

            subject = MultiValueGuc(file_seg_guc, db_seg_guc)
            error_message = "expected file value: %r to be equal to db value: %r" % (case['file_value'], case['db_value'])
            self.assertEqual(subject.is_internally_consistent(), True, error_message)

    def test_is_internally_consistent_when_gucs_are_different_returns_false(self):
        file_seg_guc = FileSegmentGuc(['contentid', 'guc_name', "'hello", "dbid"])
        db_seg_guc = DatabaseSegmentGuc(['contentid', 'guc_name', "hello"])

        subject = MultiValueGuc(file_seg_guc, db_seg_guc)
        self.assertFalse(subject.is_internally_consistent())

    def test__unquote(self):
        cases = [
            ('hello', 'hello'),
            ("''", ''),
            ("'hello'", 'hello'),
            ("'a\\b\\f\\n\\r\\tb'", 'a\b\f\n\r\tb'),
            ("'\\0\\1\\2\\3\\4\\5\\6\\7\\8\\9'", '\0\1\2\3\4\5\6\789'),
            ("'\\1\\01\\001\\0001'", '\x01\x01\x01\x001'),
            ("'\\1a1'", '\x01a1'),
            ("'\\377\\400\\776\\7777'", '\xFF\x00\xFE\xFF7'),
            ("''''", "'"),
        ]

        for quoted, unquoted in cases:
            self.assertEqual(MultiValueGuc._unquote(quoted), unquoted)

    def test__unquote_failure_cases(self):
        cases = [
            "'hello",
            "",
            "'",
            "'hello\\'",
            "'hel'lo'",
            "'''",
        ]

        for quoted in cases:
            with self.assertRaises(MultiValueGuc.ParseError):
                MultiValueGuc._unquote(quoted)

    def test_set_file_segment_succeeds(self):
        row = ['contentid', 'guc_name', 'file_value', "diff_dbid"]
        file_seg_guc = FileSegmentGuc(row)

        self.subject.set_mirror_file_segment(file_seg_guc)

        self.assertEqual(self.subject.mirror_file_seg_guc, file_seg_guc)

    def test_get_value_returns_unique(self):
        self.assertEqual(self.subject.get_value(), "sql_value||file_value")
