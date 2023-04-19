from mock import Mock, patch, call

from gppylib.commands import unix
from gppylib.commands.base import CommandResult
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests


class UnixCommandTestCase(GpTestCase):
    def setUp(self):
        self.subject = unix
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.apply_patches([
            patch('gppylib.commands.unix.check_pid_on_remotehost'),
            patch('gppylib.commands.unix.Command')
        ])

        self.mock_check_pid = self.get_mock_from_apply_patch('check_pid_on_remotehost')
        self.mock_cmd = self.get_mock_from_apply_patch('Command')

    def tearDown(self):
        super(UnixCommandTestCase, self).tearDown()

    def test_kill_9_segment_processes_info_msg(self):
        self.subject.kill_9_segment_processes('/data/primary/gpseg0', [], 'sdw1')
        self.subject.logger.info.assert_called_once_with('Terminating processes for segment /data/primary/gpseg0')

    def test_kill_9_segment_processes_empty_pid_list(self):
        self.subject.kill_9_segment_processes('/data/primary/gpseg0', [], 'sdw1')

        self.subject.logger.info.assert_called_once_with('Terminating processes for segment /data/primary/gpseg0')
        self.assertFalse(self.mock_cmd.called)

    def test_kill_9_segment_processes_multiple_pids(self):
        self.mock_check_pid.return_value = True
        self.subject.kill_9_segment_processes('/data/primary/gpseg0', [123, 456], 'sdw1')

        call_expected = [call('kill -9 process', 'kill -9 123', ctxt=2, remoteHost='sdw1'),
                         call('kill -9 process', 'kill -9 456', ctxt=2, remoteHost='sdw1')]

        self.subject.logger.info.assert_called_once_with('Terminating processes for segment /data/primary/gpseg0')
        self.assertEqual(self.mock_cmd.call_count, 2)
        self.assertIn(call_expected, self.mock_cmd.call_args_list)

    def test_kill_9_segment_processes_some_pids_do_not_exist(self):
        self.mock_check_pid.side_effect = [True, False, True]
        self.subject.kill_9_segment_processes('/data/primary/gpseg0', [123, 456, 789], 'sdw1')
        self.subject.logger.info.assert_called_once_with('Terminating processes for segment /data/primary/gpseg0')

        call_expected = [call('kill -9 process', 'kill -9 123', ctxt=2, remoteHost='sdw1'),
                         call('kill -9 process', 'kill -9 789', ctxt=2, remoteHost='sdw1')]

        self.assertEqual(self.mock_cmd.call_count, 2)
        self.assertIn(call_expected, self.mock_cmd.call_args_list)

    def test_kill_9_segment_processes_kill_error(self):
        self.mock_check_pid.return_value = True
        mc = self.mock_cmd.return_value
        mc.get_results.side_effect = [CommandResult(0, b"", b"", True, False),
                                                                     CommandResult(0, b"", b"", True, False),
                                                                     CommandResult(1, b"", b"Kill Error", False, False),
                                                                     CommandResult(1, b"", b"Kill Error", False, False)]
        self.subject.kill_9_segment_processes('/data/primary/gpseg0', [123, 456, 789], 'sdw1')

        self.subject.logger.info.assert_called_once_with('Terminating processes for segment /data/primary/gpseg0')
        self.subject.logger.error.assert_called_once_with('Failed to kill process 789 for segment /data/primary/gpseg0: Kill Error')

if __name__ == '__main__':
    run_tests()
