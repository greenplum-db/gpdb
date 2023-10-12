from mock import *

from gp_unittest import *
from gppylib.gparray import GpArray, Segment
from gppylib.commands.base import CommandResult
from gppylib.operations.rebalanceSegments import GpSegmentRebalanceOperation
from gppylib.operations.rebalanceSegments import replay_lag


class RebalanceSegmentsTestCase(GpTestCase):
    def setUp(self):
        self.pool = Mock()
        self.pool.getCompletedItems.return_value = []

        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.apply_patches([
            patch("gppylib.commands.base.WorkerPool.__init__", return_value=None),
            patch("gppylib.commands.base.WorkerPool", return_value=self.pool),
            patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram'),
            patch('gppylib.operations.rebalanceSegments.logger', return_value=mock_logger),
            patch('gppylib.db.dbconn.connect', autospec=True),
            patch('gppylib.db.dbconn.execSQLForSingleton', return_value='5678')
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

        self.subject = GpSegmentRebalanceOperation(Mock(), self._create_gparray_with_2_primary_2_mirrors(), 1, 1, False,
                                                   10)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.mock_logger = self.get_mock_from_apply_patch('logger')

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

    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value='56780000000')
    def test_rebalance_returns_warning(self, mock1):
        with self.assertRaises(Exception) as ex:
            self.subject.rebalance()
        self.assertEqual('56780000000 bytes of xlog is still to be replayed on mirror with dbid 2, let mirror catchup '
                         'on replay then trigger rebalance. Use --replay-lag to configure the allowed replay lag limit '
                         'or --disable-replay-lag to disable the check completely if you wish to continue with '
                         'rebalance anyway', str(ex.exception))
        self.assertEqual([call("Get replay lag on mirror of primary segment with host:sdw1, port:40000")],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual([call("Determining primary and mirror segment pairs to rebalance"),
                          call('Allowed replay lag during rebalance is 10 GB')],
                         self.subject.logger.info.call_args_list)

    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value='5678000000')
    def test_rebalance_does_not_return_warning(self, mock1):
        self.subject.rebalance()
        self.assertEqual([call("Get replay lag on mirror of primary segment with host:sdw1, port:40000")],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.dbconn.execSQLForSingleton', return_value='56780000000')
    def test_rebalance_replay_lag_is_disabled(self, mock1):
        self.subject.disable_replay_lag = True
        self.subject.rebalance()
        self.assertNotIn([call("Get replay lag on mirror of primary segment with host:sdw1, port:40000")],
                         self.mock_logger.debug.call_args_list)
        self.assertIn([call("Determining primary and mirror segment pairs to rebalance")],
                      self.subject.logger.info.call_args_list)

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_replay_lag_connect_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            replay_lag(self.primary0)
        self.assertEqual('Failed to query pg_stat_replication for host:sdw1, port:40000, error: ', str(ex.exception))

    @patch('gppylib.db.dbconn.execSQLForSingleton', side_effect=Exception())
    def test_replay_lag_query_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            replay_lag(self.primary0)
        self.assertEqual('Failed to query pg_stat_replication for host:sdw1, port:40000, error: ', str(ex.exception))

    def _create_gparray_with_2_primary_2_mirrors(self):
        master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        self.primary0 = Segment.initFromString(
            "2|0|p|m|s|u|sdw1|sdw1|40000|/data/primary0")
        primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        self.mirror0 = Segment.initFromString(
            "4|0|m|p|s|u|sdw2|sdw2|50000|/data/mirror0")
        mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
        return GpArray([master, self.primary0, primary1, self.mirror0, mirror1])


if __name__ == '__main__':
    run_tests()
