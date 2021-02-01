from gpconfig_modules.compare_segment_guc import MultiValueGuc
from gpconfig_modules.database_segment_guc import DatabaseSegmentGuc


class GucCollection:
    """
    provide an enhanced dict of gucs, with responsibilities to assemble sets of info per segment.

    """
    COORDINATOR_KEY = '-1'

    def __init__(self):
        self.gucs = {}

    def update_list(self, guc_list):
        for guc in guc_list:
            self.update(guc)

    def update(self, guc):
        existing = self.gucs.get(guc.context)
        if existing:
            if isinstance(existing, DatabaseSegmentGuc) and type(guc) == type(existing):
                pass  # discard mirror
            else:
                self.gucs[guc.context] = MultiValueGuc(existing, guc)
        else:
            self.gucs[guc.context] = guc

    def are_segments_consistent(self):
        self.validate()

        if not self._check_consistency_within_single_segment():
            return False

        if not self._check_consistency_across_segments():
            return False

        return True

    def _check_consistency_across_segments(self):
        segments_only = [v for k, v in self.gucs.items() if self.COORDINATOR_KEY != k]
        segment_values = [guc.get_value() for guc in segments_only]
        if len(set(segment_values)) > 1:
            return False
        return True

    def _check_consistency_within_single_segment(self):
        for guc in list(self.gucs.values()):
            if not guc.is_internally_consistent():
                return False
        return True

    def validate(self):
        if len(self.gucs) < 2 or self.COORDINATOR_KEY not in self.gucs:
            raise Exception("Collections must have at least a coordinator and segment value")

    def values(self):
        return sorted(list(self.gucs.values()), key=lambda x: x.context)

    def report(self):
        self.validate()

        if self.are_segments_consistent():
            last_seg_key = sorted(list(self.gucs.keys()), reverse=True)[0]
            report = [self.gucs[self.COORDINATOR_KEY].report_success_format(),
                      self.gucs[last_seg_key].report_success_format()]
            return "\n".join(report)
        else:
            sorted_gucs = sorted(list(self.gucs.values()), key=lambda x: x.context)
            report = []
            for guc in sorted_gucs:
                report.extend(guc.report_fail_format())
        return "\n".join(report)
