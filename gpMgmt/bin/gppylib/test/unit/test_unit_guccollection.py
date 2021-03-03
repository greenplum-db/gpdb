from mock import *
from .gp_unittest import *
from gpconfig_modules.database_segment_guc import DatabaseSegmentGuc
from gpconfig_modules.file_segment_guc import FileSegmentGuc
from gpconfig_modules.guc_collection import GucCollection


class GucCollectionTest(GpTestCase):
    def setUp(self):
        self.subject = GucCollection()
        row = ['-1', 'guc_name', 'coordinator_value']
        self.db_seg_guc_1 = DatabaseSegmentGuc(row)
        self.subject.update(self.db_seg_guc_1)
        row = ['0', 'guc_name', 'value']
        self.db_seg_guc_2 = DatabaseSegmentGuc(row)
        self.subject.update(self.db_seg_guc_2)

    def test_when_non_matching_file_value_yields_failure_format(self):
        row = ['-1', 'guc_name', 'coordinator_file_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'file_value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("[context: -1] [dbid: dbid1] [name: guc_name] [value: coordinator_value | file: coordinator_file_value]"
                      "\n[context: 0] [dbid: dbid2] [name: guc_name] [value: value | file: file_value]",
                      self.subject.report())

    def test_when_the_coordinator_is_different_from_segment_yet_all_segments_same_yields_success_format(self):
        row = ['0', 'guc_name', 'value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value | file: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value | file: value", self.subject.report())

    def test_less_than_two_segments_raises(self):
        del self.subject.gucs['0']

        with self.assertRaisesRegex(Exception,
                                     "Collections must have at least a coordinator and segment value"):
            self.subject.report()

    def test_when_invalid_gucs_with_no_coordinator_raises(self):
        del self.subject.gucs['-1']
        with self.assertRaisesRegex(Exception,
                                     "Collections must have at least a coordinator and segment value"):
            self.subject.validate()

    def test_when_three_segments_match_success_format(self):
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        row = ['1', 'guc_name', 'value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['1', 'guc_name', 'value', 'dbid3']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value | file: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value | file: value", self.subject.report())

    def test_file_format_succeeds(self):
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value", self.subject.report())

    def test_database_format_succeeds(self):
        self.assertIn("Coordinator value: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value", self.subject.report())

    def test_update_adds_to_empty(self):
        self.assertEqual(len(self.subject.gucs), 2)

    def test_update_when_same_database_segment_guc_type_overrides_existing(self):
        row = ['-1', 'guc_name', 'new_value', 'dbid']
        self.subject.update(DatabaseSegmentGuc(row))
        self.assertEqual(len(self.subject.gucs), 2)

    def test_update_after_having_full_comparison_for_a_given_contentid_succeeds(self):
        row = ['0', 'guc_name', 'value', 'dbid']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value | file: coordinator_value\nSegment     value: value | file: value", self.subject.report())

    def test_when_file_value_empty_file_compare_succeeds(self):
        row = ['0', 'guc_name', None, 'dbid']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', None, 'dbid']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value | not set in file", self.subject.report())
        self.assertIn("Segment     value: value | not set in file", self.subject.report())

    def test_values_unset_in_file_only(self):
        self.subject = GucCollection() # remove existing DatabaseSegmentGucs

        row = ['0', 'guc_name', None, 'dbid']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', None, 'dbid']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("No value is set on coordinator", self.subject.report())
        self.assertIn("No value is set on segments", self.subject.report())

    def test_when_multiple_dbids_per_contentid_reports_failure(self):
        row = ['-1', 'guc_name', 'coordinator_value', '1']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', '2']
        self.subject.update(FileSegmentGuc(row))

        row = ['0', 'guc_name', 'value', '3']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', '4']
        self.subject.update(FileSegmentGuc(row))

        row = ['1', 'guc_name', 'value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['1', 'guc_name', 'different', '5']
        self.subject.update(FileSegmentGuc(row))
        row = ['1', 'guc_name', 'different', '6']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn(""
                      "[context: -1] [dbid: 1] [name: guc_name] [value: coordinator_value | file: coordinator_value]"
                      "\n[context: -1] [dbid: 2] [name: guc_name] [value: coordinator_value | file: coordinator_value]",
                      # "\n[context: 0] [dbid: dbid] [name: guc_name] [value: value | file: file_value]",
                      self.subject.report())

    def test_update_when_file_segment_has_same_dbid_overwrites_and_succeeds(self):
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid']
        self.subject.update(FileSegmentGuc(row))
        primary_file_seg = FileSegmentGuc(row)
        self.subject.update(primary_file_seg)

        self.assertEqual(self.subject.gucs["-1"].primary_file_seg_guc, primary_file_seg)
        self.assertEqual(self.subject.gucs["-1"].mirror_file_seg_guc, None)

    def test_update_when_file_segments_first_succeeds(self):
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        primary_file_seg = FileSegmentGuc(row)
        self.subject.update(primary_file_seg)
        row = ['-1', 'guc_name', 'coordinator_value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['0', 'guc_name', 'value']
        self.subject.update(DatabaseSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value | file: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value | file: value", self.subject.report())

    def test_update_when_only_database_segments_succeeds(self):
        row = ['-1', 'guc_name', 'coordinator_value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['0', 'guc_name', 'value']
        self.subject.update(DatabaseSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value", self.subject.report())

    def test_update_when_only_file_segments_overwriting_succeeds(self):
        self.subject = GucCollection()
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value", self.subject.report())

    def test_update_when_only_file_segments_primary_and_mirror_succeeds(self):
        self.subject = GucCollection()
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid3']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid4']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("Coordinator value: coordinator_value", self.subject.report())
        self.assertIn("Segment     value: value", self.subject.report())

    def test_update_when_only_file_segments_primary_and_mirror_fails(self):
        self.subject = GucCollection()
        row = ['-1', 'guc_name', None, 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid3']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid4']
        self.subject.update(FileSegmentGuc(row))

        self.assertIn("[context: -1] [dbid: dbid1] [name: guc_name] [not set in file]\n", self.subject.report())
        self.assertIn("[context: -1] [dbid: dbid3] [name: guc_name] [value: coordinator_value]\n", self.subject.report())
        self.assertIn("[context: 0] [dbid: dbid2] [name: guc_name] [value: value]\n", self.subject.report())
        self.assertIn("[context: 0] [dbid: dbid4] [name: guc_name] [value: value]", self.subject.report())

    def test_for_sorting_based_on_context_and_dbid_with_file_option_succeeds(self):
        self.subject = GucCollection()
        row = ['-1', 'guc_name', None, 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid3']
        self.subject.update(FileSegmentGuc(row))
        row = ['1', 'guc_name', 'value', 'dbid6']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid4']
        self.subject.update(FileSegmentGuc(row))
        row = ['1', 'guc_name', 'value', 'dbid5']
        self.subject.update(FileSegmentGuc(row))

        self.assertEqual("[context: -1] [dbid: dbid1] [name: guc_name] [not set in file]\n"
                          "[context: -1] [dbid: dbid3] [name: guc_name] [value: coordinator_value]\n"
                          "[context: 0] [dbid: dbid2] [name: guc_name] [value: value]\n"
                          "[context: 0] [dbid: dbid4] [name: guc_name] [value: value]\n"
                          "[context: 1] [dbid: dbid5] [name: guc_name] [value: value]\n"
                          "[context: 1] [dbid: dbid6] [name: guc_name] [value: value]", self.subject.report())

    def test_for_sorting_based_on_context_and_dbid_with_file_compare_option_succeeds(self):
        self.subject = GucCollection()
        row = ['-1', 'guc_name', 'coordinator_value', 'dbid1']
        self.subject.update(FileSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid2']
        self.subject.update(FileSegmentGuc(row))
        row = ['-1', 'guc_name', 'coordinator_value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['0', 'guc_name', 'value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['1', 'guc_name', 'value', 'dbid5']
        self.subject.update(FileSegmentGuc(row))
        row = ['1', 'guc_name', 'wrong_value', 'dbid4']
        self.subject.update(FileSegmentGuc(row))
        row = ['1', 'guc_name', 'value']
        self.subject.update(DatabaseSegmentGuc(row))
        row = ['0', 'guc_name', 'value', 'dbid3']
        self.subject.update(FileSegmentGuc(row))

        self.assertEqual("[context: -1] [dbid: dbid1] [name: guc_name] [value: coordinator_value | file: coordinator_value]\n"
                          "[context: 0] [dbid: dbid2] [name: guc_name] [value: value | file: value]\n"
                          "[context: 0] [dbid: dbid3] [name: guc_name] [value: value | file: value]\n"
                          "[context: 1] [dbid: dbid4] [name: guc_name] [value: value | file: wrong_value]\n"
                          "[context: 1] [dbid: dbid5] [name: guc_name] [value: value | file: value]", self.subject.report())

    def test_values_succeeds(self):
        self.assertEqual([self.db_seg_guc_1, self.db_seg_guc_2], list(self.subject.values()))
