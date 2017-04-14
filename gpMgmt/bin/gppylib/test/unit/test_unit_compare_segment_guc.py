from mock import *
from gp_unittest import *
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

        self.assertEquals(self.subject.db_seg_guc, old.db_seg_guc)
        self.assertEquals(self.subject.primary_file_seg_guc, old.primary_file_seg_guc)
        self.assertEquals(self.subject.mirror_file_seg_guc, file_seg_guc)

    def test_init_with_wrong_content_id_raises(self):
        row = ['contentid', 'guc_name', 'file_value', "dbid"]
        file_seg_guc = FileSegmentGuc(row)
        row = ['different', 'guc_name', 'sql_value']
        db_seg_guc = DatabaseSegmentGuc(row)

        with self.assertRaisesRegexp(Exception, "Not the same context"):
            MultiValueGuc(file_seg_guc, db_seg_guc)

    def test_init_handles_both_orders(self):
        self.assertEquals(self.file_seg_guc, self.subject.primary_file_seg_guc)
        self.assertEquals(self.db_seg_guc, self.subject.db_seg_guc)
        self.assertTrue(isinstance(self.subject.primary_file_seg_guc, FileSegmentGuc))
        self.assertTrue(isinstance(self.subject.db_seg_guc, DatabaseSegmentGuc))

        self.subject = MultiValueGuc(self.db_seg_guc, self.file_seg_guc)

        self.assertEquals(self.file_seg_guc, self.subject.primary_file_seg_guc)
        self.assertEquals(self.db_seg_guc, self.subject.db_seg_guc)
        self.assertTrue(isinstance(self.subject.primary_file_seg_guc, FileSegmentGuc))
        self.assertTrue(isinstance(self.subject.db_seg_guc, DatabaseSegmentGuc))

    def test_init_when_none_raises(self):
        with self.assertRaisesRegexp(Exception, "comparison requires two gucs"):
            self.subject = MultiValueGuc(self.db_seg_guc, None)

        with self.assertRaisesRegexp(Exception, "comparison requires two gucs"):
            self.subject = MultiValueGuc(None, self.db_seg_guc)

    def test_report_fail_format_for_database_and_file_gucs(self):
        self.assertEquals(self.subject.report_fail_format(),
                          ["[context: contentid] [dbid: dbid] [name: guc_name] [value: sql_value | file: file_value]"])

    def test_report_fail_format_file_segment_guc_only(self):
        self.subject.db_seg_guc = None
        row = ['contentid', 'guc_name', 'primary_value', "dbid1"]
        self.subject.set_primary_file_segment(FileSegmentGuc(row))
        row = ['contentid', 'guc_name', 'mirror_value', "dbid2"]
        self.subject.set_mirror_file_segment(FileSegmentGuc(row))
        self.assertEquals(self.subject.report_fail_format(),
                          ["[context: contentid] [dbid: dbid1] [name: guc_name] [value: primary_value]",
                          "[context: contentid] [dbid: dbid2] [name: guc_name] [value: mirror_value]"])

    def test_when_segment_report_success_format(self):
        self.assertEquals(self.subject.report_success_format(),
                          "Segment value: sql_value | file: file_value")

    def test_when_values_match_report_success_format_file_compare(self):
        self.subject.db_seg_guc.value = 'value'
        self.subject.primary_file_seg_guc.value = 'value'
        self.assertEquals(self.subject.report_success_format(), "Segment value: value | file: value")

    def test_is_internally_consistent_fails(self):
        self.assertEquals(self.subject.is_internally_consistent(), False)

    def test_is_internally_consistent_when_file_value_is_none_succeeds(self):
        self.file_seg_guc.value = None
        self.assertEquals(self.subject.is_internally_consistent(), True)

    def test_is_internally_consistent_when_primary_is_same_succeeds(self):
        self.subject.primary_file_seg_guc.value = "sql_value"
        self.assertEquals(self.subject.is_internally_consistent(), True)

    def test_is_internally_consistent_when_mirror_is_different_fails(self):
        self.subject.primary_file_seg_guc.value = "sql_value"
        row = ['contentid', 'guc_name', 'diffvalue', "dbid1"]
        self.subject.set_mirror_file_segment(FileSegmentGuc(row))
        self.assertEquals(self.subject.is_internally_consistent(), False)

    def test_set_file_segment_succeeds(self):
        row = ['contentid', 'guc_name', 'file_value', "diff_dbid"]
        file_seg_guc = FileSegmentGuc(row)

        self.subject.set_mirror_file_segment(file_seg_guc)

        self.assertEquals(self.subject.mirror_file_seg_guc, file_seg_guc)

    def test_get_value_returns_unique(self):
        self.assertEquals(self.subject.get_value(), "sql_value||file_value")
