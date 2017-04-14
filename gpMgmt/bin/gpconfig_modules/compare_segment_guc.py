from gpconfig_modules.database_segment_guc import DatabaseSegmentGuc
from gpconfig_modules.file_segment_guc import FileSegmentGuc
from gpconfig_modules.segment_guc import SegmentGuc


class MultiValueGuc(SegmentGuc):
    """
    encapsulate various GUC locations within a given segment.
    A segment can include 2 databases: the primary and a mirror.
    The database value is singular, since we strongly expect the values to be the same, given mirroring.
    However, the file values of primary and mirror can be different. 
    So we model this MultiValueGuc object to accept 2 file values, and one database value.
    """
    def __init__(self, guc1, guc2):
        """
        accept 2 gucs in any order. gucs can be any combination of:

        * database guc
        * file guc
            - primary
            - mirror
        * existing comparison guc, with or without mirror
        """
        self.primary_file_seg_guc = None
        self.mirror_file_seg_guc = None
        self.db_seg_guc = None

        if not guc1 or not guc2:
            raise Exception("comparison requires two gucs")

        SegmentGuc.__init__(self, [guc1.context, guc1.name])
        if guc1.context != guc2.context:
            raise Exception("Not the same context")

        if isinstance(guc1, MultiValueGuc):
            # copy constructor
            self.db_seg_guc = guc1.db_seg_guc
            self.primary_file_seg_guc = guc1.primary_file_seg_guc
            self.mirror_file_seg_guc = guc1.mirror_file_seg_guc

        if isinstance(guc2, MultiValueGuc):
            # copy constructor
            self.db_seg_guc = guc2.db_seg_guc
            self.primary_file_seg_guc = guc2.primary_file_seg_guc
            self.mirror_file_seg_guc = guc2.mirror_file_seg

        if isinstance(guc1, FileSegmentGuc):
            if self.primary_file_seg_guc:
                if self.primary_file_seg_guc.dbid == guc1.dbid:
                    self.primary_file_seg_guc = guc1
                else:
                    self.mirror_file_seg_guc = guc1
            else:
                self.primary_file_seg_guc = guc1

        if isinstance(guc2, FileSegmentGuc):
            if self.primary_file_seg_guc:
                if self.primary_file_seg_guc.dbid == guc2.dbid:
                    self.primary_file_seg_guc = guc2
                else:
                    self.mirror_file_seg_guc = guc2
            else:
                self.primary_file_seg_guc = guc2

        if isinstance(guc1, DatabaseSegmentGuc):
            self.db_seg_guc = guc1

        if isinstance(guc2, DatabaseSegmentGuc):
            self.db_seg_guc = guc2

    def report_success_format(self):
        file_val = self.primary_file_seg_guc.get_value()
        if self.db_seg_guc:
            result = "%s value: %s | file: %s" % (self.get_label(), self.db_seg_guc.value, self._use_dash_when_none(file_val))
        else:
            result = "%s value: %s" % (self.get_label(), file_val)
        return result

    def report_fail_format(self):
        sort_seg_guc_objs = [obj for obj in [self.primary_file_seg_guc, self.mirror_file_seg_guc] if obj]
        sort_seg_guc_objs.sort(key=lambda x: x.dbid)

        if self.db_seg_guc:
            report = [self._report_fail_format_with_database_and_file_gucs(seg_guc_obj) for seg_guc_obj in sort_seg_guc_objs]
        else:
            report = [seg_guc_obj.report_fail_format()[0] for seg_guc_obj in sort_seg_guc_objs]
        return report

    def _report_fail_format_with_database_and_file_gucs(self, segment_guc_obj):
        return "[context: %s] [dbid: %s] [name: %s] [value: %s | file: %s]" % (
            self.db_seg_guc.context,
            segment_guc_obj.dbid,
            self.db_seg_guc.name,
            self.db_seg_guc.value,
            self._use_dash_when_none(segment_guc_obj.value))

    def _use_dash_when_none(self, value):
      return value if value is not None else "-"


    def is_internally_consistent(self):
        if not self.db_seg_guc:
            return self.compare_primary_and_mirror_files()
        else:
            if self.primary_file_seg_guc is None:
                return True
            if self.primary_file_seg_guc.get_value() is None:
                return True
            result = True
            if self.mirror_file_seg_guc and self.db_seg_guc:
                result = self.mirror_file_seg_guc.value == self.db_seg_guc.value
            if not result:
                return result
            return self.db_seg_guc.value == self.primary_file_seg_guc.value and result

    def get_value(self):
        file_value = ""
        if self.primary_file_seg_guc:
            file_value = str(self.primary_file_seg_guc.get_value())
        db_value = ""
        if self.db_seg_guc:
            db_value = str(self.db_seg_guc.get_value())

        return db_value + "||" + file_value

    def set_mirror_file_segment(self, mirror_file_seg):
        self.mirror_file_seg_guc = mirror_file_seg

    def get_primary_dbid(self):
        return self.primary_file_seg_guc.dbid

    def set_primary_file_segment(self, guc):
        self.primary_file_seg_guc = guc

    def compare_primary_and_mirror_files(self):
        if self.primary_file_seg_guc and self.mirror_file_seg_guc:
            return self.primary_file_seg_guc.get_value() == self.mirror_file_seg_guc.get_value()
        return True
