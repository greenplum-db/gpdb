from gpconfig_modules.segment_guc import SegmentGuc


class DatabaseSegmentGuc(SegmentGuc):

    def __init__(self, row):
        SegmentGuc.__init__(self, row)

        if len(row) < 3:
            raise Exception("must provide ['context', 'guc name', 'value']")

        self.value = row[2]

    def report_success_format(self):
        if self.get_value() is not None:
            return "%s value: %s" % (self.get_label(), self.get_value())
        return "No value is set on %s" % ("coordinator" if self.get_label() == "Coordinator" else "segments")

    def report_fail_format(self):
        return ["[context: %s] [name: %s] [value: %s]" % (self.context, self.name, self.get_value())]

    def is_internally_consistent(self):
        return True

    def get_value(self):
        return self.value
