from mock import *

from gp_unittest import *
from gppylib.gparray import GpArray, GpDB
from gppylib.commands.base import CommandResult
from gppylib.operations.rebalanceSegments import GpSegmentRebalanceOperation


class RebalanceSegmentsTestCase(GpTestCase):
    def setUp(self):
        self.pool = Mock()
        self.pool.getCompletedItems.return_value = []

        self.apply_patches([
            patch("gppylib.commands.base.WorkerPool.__init__", return_value=None),
            patch("gppylib.commands.base.WorkerPool", return_value=self.pool),
            patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram'),
        ])

        self.mock_gp_recover_segment_prog_class = self.get_mock_from_apply_patch('GpRecoverSegmentProgram')
        self.mock_parser = Mock()
        self.mock_gp_recover_segment_prog_class.createParser.return_value = self.mock_parser
        self.mock_parser.parse_args.return_value = (Mock(), Mock())
        self.mock_gp_recover_segment_prog = Mock()
        self.mock_gp_recover_segment_prog_class.createProgram.return_value = self.mock_gp_recover_segment_prog

        self.failure_command_mock = Mock()
        self.failure_command_mock.get_results.return_value = CommandResult(
            1, "stdout failure text", "stderr text", True, False)

        self.success_command_mock = Mock()
        self.success_command_mock.get_results.return_value = CommandResult(
            0, "stdout success text", "stderr text", True, False)

        self.subject = GpSegmentRebalanceOperation(Mock(), self._create_gparray_with_2_primary_2_mirrors())
        self.subject.logger = Mock()

    def tearDown(self):
        super(RebalanceSegmentsTestCase, self).tearDown()

    def test_rebalance_returns_success(self):
        self.pool.getCompletedItems.return_value = [self.success_command_mock]

        result = self.subject.rebalance()

        self.assertTrue(result)

    def test_rebalance_raises(self):
        self.pool.getCompletedItems.return_value = [self.failure_command_mock]
        self.mock_gp_recover_segment_prog.run.side_effect = SystemExit(1)

        with self.assertRaisesRegexp(Exception, "Error synchronizing."):
            self.subject.rebalance()

    def test_rebalance_returns_failure(self):
        self.pool.getCompletedItems.side_effect = [[self.failure_command_mock], [self.success_command_mock]]

        result = self.subject.rebalance()
        self.assertFalse(result)

    def _create_gparray_with_2_primary_2_mirrors(self):
        master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        self.primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        primary1 = GpDB.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        self.mirror0 = GpDB.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        mirror1 = GpDB.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        return GpArray([master, self.primary0, primary1, self.mirror0, mirror1])


if __name__ == '__main__':
    run_tests()
