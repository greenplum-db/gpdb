from mock import Mock, patch, call

from gppylib.test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.commands.base import CommandResult, ExecutionError
from gppylib.programs.clsRecoverSegment import GpRecoverSegmentProgram

class RecoverSegmentsTestCase(GpTestCase):
    def setUp(self):
        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.apply_patches([
            patch('gppylib.programs.clsRecoverSegment.logger', return_value=mock_logger),
        ])

        self.mock_logger = self.get_mock_from_apply_patch('logger')

        # Mock WorkerPool
        self.mock_pool = Mock()
        self.mock_pool.isDone.side_effect = [False, True]
        self.obj = GpRecoverSegmentProgram(Mock())
        self.obj._GpRecoverSegmentProgram__pool = self.mock_pool
    
    def tearDown(self):
        super(RecoverSegmentsTestCase, self).tearDown()

    @patch('gppylib.programs.clsRecoverSegment.Command.run') 
    def test_shutdown_runs_successfully_single_host(self, mock1):
        self.obj.shutdown(['sdw1'])

        self.mock_logger.debug.assert_called_once_with("Terminating recovery process on host sdw1")
        self.assertEqual(mock1.call_count, 1)
    
    @patch('gppylib.programs.clsRecoverSegment.Command.run')
    def test_shutdown_runs_successfully_multiple_hosts(self, mock1):
        self.obj.shutdown(['sdw1', 'sdw2', 'sdw3'])

        self.mock_logger.debug.assert_any_call("Terminating recovery process on host sdw1")
        self.mock_logger.debug.assert_any_call("Terminating recovery process on host sdw2")
        self.mock_logger.debug.assert_any_call("Terminating recovery process on host sdw3")

        self.assertEqual(mock1.call_count, 3)

    @patch('gppylib.programs.clsRecoverSegment.ExecutionError.__str__', return_value="Error getting recovery PID")
    def test_shutdown_logs_exception_on_single_host(self, mock1):

        def mock_func(*args, **kwargs):
            cmd = args[0]
            if cmd.remoteHost == "sdw2":
                raise ExecutionError("Error getting recovery PID", cmd)

        with patch('gppylib.programs.clsRecoverSegment.Command.run', mock_func):
            self.obj.shutdown(['sdw1', 'sdw2', 'sdw3'])

        self.mock_logger.debug.assert_has_calls([call("Terminating recovery process on host sdw1"),
                                                 call("Terminating recovery process on host sdw2"),
                                                 call("Terminating recovery process on host sdw3")])
        self.mock_logger.error.assert_called_once_with("Not able to terminate recovery process on host sdw2: Error getting recovery PID")

    @patch('gppylib.programs.clsRecoverSegment.ExecutionError.__str__', return_value="Error getting recovery PID")
    def test_shutdown_logs_exception_on_multiple_host(self, mock1):

        def mock_func(*args, **kwargs):
            cmd = args[0]
            if cmd.remoteHost in ["sdw1", "sdw3"]:
                raise ExecutionError("Error getting recovery PID", cmd)

        with patch('gppylib.programs.clsRecoverSegment.Command.run', mock_func):
            self.obj.shutdown(['sdw1', 'sdw2', 'sdw3'])

        self.mock_logger.debug.assert_has_calls([call("Terminating recovery process on host sdw1"),
                                                 call("Terminating recovery process on host sdw2"),
                                                 call("Terminating recovery process on host sdw3")])
        self.mock_logger.error.assert_has_calls([call("Not able to terminate recovery process on host sdw1: Error getting recovery PID"),
                                                 call("Not able to terminate recovery process on host sdw3: Error getting recovery PID")])


if __name__ == '__main__':
    run_tests()
