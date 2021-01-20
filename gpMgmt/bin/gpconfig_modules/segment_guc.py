import abc


class SegmentGuc(metaclass=abc.ABCMeta):
    COORDINATOR_CONTEXT = '-1'

    def __init__(self, row):
        self.context = str(row[0])
        self.name = row[1]

    def get_label(self):
        label = "Coordinator" if self.context == self.COORDINATOR_CONTEXT else "Segment    "
        return label

    @abc.abstractmethod
    def report_success_format(self):
        raise NotImplementedError('users must define __str__ to use this base class')

    @abc.abstractmethod
    def report_fail_format(self):
        raise NotImplementedError('users must define __str__ to use this base class')

    @abc.abstractmethod
    def is_internally_consistent(self):
        raise NotImplementedError('users must define __str__ to use this base class')

    @abc.abstractmethod
    def get_value(self):
        raise NotImplementedError('users must define __str__ to use this base class')
