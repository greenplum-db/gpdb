from gpconfig_modules.segment_guc import SegmentGuc


class FileSegmentGuc(SegmentGuc):

    def __init__(self, row):
        SegmentGuc.__init__(self, row)

        if len(row) < 4:
            raise Exception("must provide ['context', 'guc name', 'value', 'dbid']")

        self.value = row[2]
        self.dbid = str(row[3])

    def report_success_format(self):
        return "%s value: %s" % (self.get_label(), self._use_dash_when_none(self.get_value()))

    def report_fail_format(self):
        return ["[context: %s] [dbid: %s] [name: %s] [value: %s]" % (self.context, self.dbid, self.name, self._use_dash_when_none(self.get_value()))]

    def is_internally_consistent(self):
        return True

    def get_value(self):
        return self.value

    def _use_dash_when_none(self, value):
        return value if value is not None else "-"

