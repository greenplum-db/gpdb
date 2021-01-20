from gpconfig_modules.segment_guc import SegmentGuc


class FileSegmentGuc(SegmentGuc):

    def __init__(self, row):
        SegmentGuc.__init__(self, row)

        if len(row) < 4:
            raise Exception("must provide ['context', 'guc name', 'value', 'dbid']")

        self.value = row[2]
        self.dbid = str(row[3])

    def report_success_format(self):
        if self.get_value() is not None:
            return "%s value: %s" % (self.get_label(), self.get_value())
        return "No value is set on %s" % ("coordinator" if self.get_label() == "Coordinator" else "segments")

    def report_fail_format(self):
        value = self.get_value()
        if value is not None:
            value = 'value: %s' % value
        else:
            value = 'not set in file'

        return ["[context: %s] [dbid: %s] [name: %s] [%s]" % (self.context, self.dbid, self.name, value)]

    def is_internally_consistent(self):
        return True

    def get_value(self):
        return self.value
