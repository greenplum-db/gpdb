import unittest
from . import redirect_stderr
from mock import call, Mock, patch, ANY
import sys

from .gp_unittest import GpTestCase
import gppylib
from gpsegrecovery import FullRecovery
from recovery_base import RecoveryBase, set_recovery_cmd_results
from gppylib.recoveryinfo import RecoveryInfo
from gppylib.commands.base import CommandResult, Command


class RecoveryBaseTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.apply_patches([
            patch('recovery_base.gplog.setup_tool_logging', return_value=self.mock_logger),
            patch('recovery_base.gplog.enable_verbose_logging'),
        ])

        self.mock_cmd1 = Mock(spec=['run'])
        self.mock_cmd2 = Mock(spec=['run'])
        self.cmd_list = [self.mock_cmd1, self.mock_cmd2]

        self.file_name = '/path/to/test_file.py'

        self.mock_setup_tool_logging = self.get_mock_from_apply_patch('setup_tool_logging')
        self.mock_enable_verbose_logging = self.get_mock_from_apply_patch('enable_verbose_logging')

        self.full_r1 = RecoveryInfo('target_data_dir1', 5001, 1, 'source_hostname1',
                                    6001, 'source_datadir1', True, False, '/tmp/progress_file1')
        self.incr_r2 = RecoveryInfo('target_data_dir2', 5002, 2, 'source_hostname2',
                                    6002, 'source_datadir2', False, False, '/tmp/progress_file2')
        self.confinfo = gppylib.recoveryinfo.serialize_list([self.full_r1,
                                                             self.incr_r2])

    def tearDown(self):
        super(RecoveryBaseTestCase, self).tearDown()

    def run_recovery_base_get_stderr(self):
        with redirect_stderr() as buf:
            with self.assertRaises(SystemExit) as ex:
                recovery_base = RecoveryBase(self.file_name)
                recovery_base.main(self.cmd_list)
        return buf, ex

    def _asserts_for_passing_tests(self, stderr_buf, ex, enable_verbose_count=0, warn_count=0):
        self.assertEqual('', stderr_buf.getvalue().strip())
        self.assertEqual(0, ex.exception.code)

        self.assertEqual(enable_verbose_count, self.mock_enable_verbose_logging.call_count)
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(0, self.mock_logger.error.call_count)
        self.assertEqual(warn_count, self.mock_logger.warn.call_count)
        self.assertEqual(0, self.mock_logger.exception.call_count)

    def _asserts_for_failing_tests(self, ex, stderr_buf, expected_message, info_count=1):
        self.assertEqual(1, ex.exception.code)
        self.assertEqual(expected_message, stderr_buf.getvalue().strip())
        self.assertEqual([call(expected_message)], self.mock_logger.error.call_args_list)
        self.assertEqual(info_count, self.mock_logger.info.call_count)

    def _assert_workerpool_calls(self, mock_workerpool):
        self.assertEqual([call(numWorkers=2)], mock_workerpool.call_args_list)
        self.assertEqual([call(self.mock_cmd1), call(self.mock_cmd2)],
                         mock_workerpool.return_value.addCommand.call_args_list)
        self.assertEqual(1, mock_workerpool.return_value.join.call_count)
        self.assertEqual(1, mock_workerpool.return_value.haltWork.call_count)

    def _assert_exception_from_parseargs(self, ex, stderr_buf, expected_stderr_message):
        stderr_buf.seek(0)
        stderr_lines = stderr_buf.readlines()
        self.assertEqual(3, len(stderr_lines))
        self.assertEqual(expected_stderr_message, stderr_lines[2])
        self.assertEqual(2, ex.exception.code)
        self.assertEqual(0, self.mock_logger.error.call_count)

    def test_confinfo_not_passed_fails(self):
        sys.argv = ['recovery_base']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self.assertEqual(1, ex.exception.code)
        self.assertEqual("Missing --confinfo argument.", stderr_buf.getvalue().strip())

    def test_logdir_not_passed_fails(self):
        sys.argv = ['recovery_base', '-c', 'test']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self.assertEqual(1, ex.exception.code)
        self.assertEqual("Missing --log-dir argument.", stderr_buf.getvalue().strip())

    def test_confinfo_passed_as_blank_fails(self):
        sys.argv = ['recovery_base', '-c']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._assert_exception_from_parseargs(ex, stderr_buf,
                                             'recovery_base: error: -c option requires an argument\n')

    def test_invalid_option_fails(self):
        sys.argv = ['recovery_base', '-z']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._assert_exception_from_parseargs(ex, stderr_buf,
                                             'recovery_base: error: no such option: -z\n')

    def test_invalid_type_batchsize_fails(self):
        sys.argv = ['recovery_base',
                    '-c {}'.format(self.confinfo),
                    '-b', 'foo']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._assert_exception_from_parseargs(ex, stderr_buf,
                                             "recovery_base: error: option -b: invalid integer value: 'foo'\n")

    def test_confinfo_passed_as_empty_fails(self):
        sys.argv = ['recovery_base', '-l', '/tmp/logdir',
                    '-c {}'.format(gppylib.recoveryinfo.serialize_list([]))]
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        stderr_buf.seek(0)
        stderr_lines = stderr_buf.readlines()
        self.assertEqual(1, len(stderr_lines))
        self.assertEqual("No segment configuration values found in --confinfo argument\n", stderr_lines[0])
        self.assertEqual(1, ex.exception.code)
        self.assertEqual(1, self.mock_logger.error.call_count)


    @patch('recovery_base.WorkerPool')
    def test_batchsize_less_than_cmdlist_passes(self, mock_workerpool):
        sys.argv = ['recovery_base', '-l', '/tmp/logdir',
                    '-c {}'.format(self.confinfo),
                    '-b 1']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_passing_tests(stderr_buf, ex)
        self.assertEqual(call(numWorkers=1), mock_workerpool.call_args)

    @patch('recovery_base.WorkerPool')
    def test_batchsize_greater_than_cmdlist_passes(self, mock_workerpool):
        sys.argv = ['recovery_base', '-l', '/tmp/logdir',
                    '-c {}'.format(self.confinfo),
                    '-b 10']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_passing_tests(stderr_buf, ex)
        self.assertEqual(call(numWorkers=2), mock_workerpool.call_args)

    @patch('recovery_base.WorkerPool')
    def test_invalid_batchsize_passes(self, mock_workerpool):
        sys.argv = ['recovery_base', '-l', '/tmp/logdir',
                    '-c {}'.format(self.confinfo),
                    '-b -1']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_passing_tests(stderr_buf, ex, warn_count=1)
        self.mock_logger.warn.assert_called_once_with(
            'batch_size was less than zero.  Setting to 1.')
        self.assertEqual(call(numWorkers=1), mock_workerpool.call_args)

    @patch('recovery_base.WorkerPool')
    @patch('recovery_base.DEFAULT_SEGHOST_NUM_WORKERS', 3)
    def test_default_batch_size_is_used_passes(self, mock_workerpool):
        # To test that our code uses the default batch size, we need to
        # make sure there are more than 3 segments to recover
        new_confinfo = gppylib.recoveryinfo.serialize_list(
            [self.full_r1, self.incr_r2, self.full_r1, self.incr_r2])
        self.cmd_list = [Mock(), Mock(), Mock(), Mock()]
        sys.argv = ['recovery_base', '-l', '/tmp/logdir',
                    '-c {}'.format(new_confinfo)]
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_passing_tests(stderr_buf, ex)
        self.assertEqual(call(numWorkers=3), mock_workerpool.call_args)

    @patch('recovery_base.WorkerPool')
    def test_valid_cmd_default_options_passes(self, mock_workerpool):
        mock_workerpool.return_value = Mock()
        cmd1 = Command('testcmd', 'testcmdstr')
        cmd1.set_results(CommandResult(0, b'', b'', True, False))
        mock_workerpool.return_value.getCompletedItems = Mock(return_value=[cmd1, cmd1])
        sys.argv = ['recovery_base',  '-l', '/tmp/logdir', '-c {}'.format(self.confinfo)]
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_passing_tests(stderr_buf, ex)
        self.assertEqual([call('test_file.py', ANY, ANY, logdir='/tmp/logdir')],
                         self.mock_setup_tool_logging.call_args_list)
        self._assert_workerpool_calls(mock_workerpool)

    @patch('recovery_base.WorkerPool')
    def test_valid_cmd_non_default_options_passes(self, mock_workerpool):
        mock_workerpool.return_value = Mock()
        cmd1 = Command('testcmd', 'testcmdstr')
        cmd1.set_results(CommandResult(0, b'', b'', True, False))
        mock_workerpool.return_value.getCompletedItems = Mock(return_value=[cmd1, cmd1])
        sys.argv = ['recovery_base', '-l', '/tmp/logdir',
                    '-c {}'.format(self.confinfo),
                    '-b 10', '-f', '-v', '--era', '1234_2021']
        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_passing_tests(stderr_buf, ex, enable_verbose_count=1)
        self.assertEqual([call('test_file.py', ANY, ANY, logdir='/tmp/logdir')],
                         self.mock_setup_tool_logging.call_args_list)
        self._assert_workerpool_calls(mock_workerpool)

    # def test_get_cmd_list_exception_fails(self):
    #     sys.argv = ['recovery_base', '-l', '/tmp/logdir', '-c {}'.format(self.confinfo)]
    #     self.mock_get_cmd_list.side_effect = [Exception('cannot get command list')]
    #     stderr_buf, ex = self.run_recovery_base_get_stderr()
    #     self.assertEqual(1, self.mock_get_cmd_list.call_count)
    #     self._asserts_for_failing_tests(ex, stderr_buf, "cannot get command list")

    def test_invalid_cmd_fails(self):
        # We use the same cmd str for both because the output can contain them
        # in any order
        cmd1 = Command('invalid_cmd1', 'invalid_cmd_str')
        cmd2 = Command('invalid_cmd2', 'invalid_cmd_str')
        self.cmd_list = [cmd1, cmd2]
        sys.argv = ['recovery_base',  '-l', '/tmp/logdir', '-c {}'.format(self.confinfo)]

        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_failing_tests(ex, stderr_buf,
                                        '[{"progress_file": null, "dbid": null, "error_type": "default", "port": null, '
                                        '"datadir": null, "error_msg": "/bin/bash: invalid_cmd_str: command not found\\n"}, '
                                        '{"progress_file": null, "dbid": null, "error_type": "default", "port": null, '
                                        '"datadir": null, "error_msg": "/bin/bash: invalid_cmd_str: command not found\\n"}]')

    #TODO do we need this test where an invalid command fails but with a wrapper error?
    # def test_invalid_cmd_fails_with_wrapper_error(self):
    #     # We use the same cmd str for both because the output can contain them
    #     # in any order
    #     cmd1 = Command('invalid_cmd1', 'invalid_cmd_str')
    #     cmd2 = Command('invalid_cmd2', 'invalid_cmd_str')
    #     self.cmd_list = [cmd1, cmd2]
    #     sys.argv = ['recovery_base',  '-l', '/tmp/logdir', '-c {}'.format(self.confinfo)]
    #
    #     stderr_buf, ex = self.run_recovery_base_get_stderr()
    #
    #     self._asserts_for_failing_tests(ex, stderr_buf, "/bin/bash: invalid_cmd_str: command not found \n"
    #                                                     "/bin/bash: invalid_cmd_str: command not found ")


    def test_invalid_cmd_verbose_fails(self):
        cmd1 = Command('invalid_cmd1', 'invalid_cmd_str')
        cmd2 = Command('invalid_cmd2', 'invalid_cmd_str')
        self.cmd_list = [cmd1, cmd2]
        sys.argv = ['recovery_base',  '-l', '/tmp/logdir', '-c {}'.format(self.confinfo), '-v']

        stderr_buf, ex = self.run_recovery_base_get_stderr()
        self._asserts_for_failing_tests(ex, stderr_buf,
                                        '[{"progress_file": null, "dbid": null, "error_type": "default", "port": null, '
                                        '"datadir": null, "error_msg": "/bin/bash: invalid_cmd_str: command not found\\n"}, '
                                        '{"progress_file": null, "dbid": null, "error_type": "default", "port": null, '
                                        '"datadir": null, "error_msg": "/bin/bash: invalid_cmd_str: command not found\\n"}]'
)

        self.assertEqual(1, self.mock_enable_verbose_logging.call_count)
        self.assertEqual(1, self.mock_logger.exception.call_count)

    def test_valid_failing_cmd_fails(self):
        cmd1 = Command('valid_failing_cmd1', 'echo 1 | grep 2')
        cmd2 = Command('valid_failing_cmd2', 'echo 1 | grep 2')
        self.cmd_list = [cmd1, cmd2]
        sys.argv = ['recovery_base',  '-l', '/tmp/logdir', '-c {}'.format(self.confinfo)]

        stderr_buf, ex = self.run_recovery_base_get_stderr()
        # The echo+grep cmd that we use has a non zero return code and no stderr.
        self._asserts_for_failing_tests(ex, stderr_buf, '[{"progress_file": null, "dbid": null, "error_type": "default",'
                                                        ' "port": null, "datadir": null, "error_msg": ""}, '
                                                        '{"progress_file": null, "dbid": null, "error_type": "default", '
                                                        '"port": null, "datadir": null, "error_msg": ""}]')

        self.assertEqual(0, self.mock_enable_verbose_logging.call_count)
        self.assertEqual(0, self.mock_logger.exception.call_count)

    def test_valid_failing_cmd_verbose_fails(self):
        cmd1 = Command('valid_failing_cmd1', 'echo 1 | grep 2')
        cmd2 = Command('valid_failing_cmd2', 'echo 1 | grep 2')
        self.cmd_list = [cmd1, cmd2]
        sys.argv = ['recovery_base',  '-l', '/tmp/logdir', '-c {}'.format(self.confinfo), '-v']

        stderr_buf, ex = self.run_recovery_base_get_stderr()
        # The echo+grep cmd that we use has a non zero return code and no stderr.
        self._asserts_for_failing_tests(ex, stderr_buf, '[{"progress_file": null, "dbid": null, "error_type": "default", '
                                                        '"port": null, "datadir": null, "error_msg": ""}, '
                                                        '{"progress_file": null, "dbid": null, "error_type": "default", '
                                                        '"port": null, "datadir": null, "error_msg": ""}]'
)

        self.assertEqual(1, self.mock_enable_verbose_logging.call_count)
        self.assertEqual(1, self.mock_logger.exception.call_count)

    @patch('recovery_base.RecoveryBase.parseargs', side_effect=Exception())
    def test_parseargs_exception_fails(self, mock_parseargs):
        sys.argv = ['recovery_base']
        with self.assertRaises(SystemExit) as ex:
            RecoveryBase(self.file_name)
        self.assertEqual(1, ex.exception.code)
        self.assertEqual(0, self.mock_logger.error.call_count)


class SetCmdResultsTestCase(GpTestCase):
    def _assert_cmd_passed(self, cmd):
        self.assertEqual(0, cmd.get_results().rc)
        self.assertEqual('', cmd.get_results().stdout)
        self.assertEqual('', cmd.get_results().stderr)
        self.assertEqual(True, cmd.get_results().completed)
        self.assertEqual(False, cmd.get_results().halt)
        self.assertEqual(True, cmd.get_results().wasSuccessful())

    def _assert_cmd_failed(self, cmd, expected_stderr):
        self.assertEqual(1, cmd.get_results().rc)
        self.assertEqual('', cmd.get_results().stdout)
        self.assertItemsEqual(expected_stderr, cmd.get_results().stderr)
        self.assertEqual(True, cmd.get_results().completed)
        self.assertEqual(False, cmd.get_results().halt)
        self.assertEqual(False, cmd.get_results().wasSuccessful())

    def test_set_recovery_cmd_results_no_exception(self):
        @set_recovery_cmd_results
        def test_decorator(cmd):
            cmd.name = 'new name'

        test_cmd = Command(name='original name', cmdStr='echo foo')
        test_cmd.error_type = 'foo'
        test_decorator(test_cmd)
        self.assertEqual('new name', test_cmd.name)
        self._assert_cmd_passed(test_cmd)

    def test_set_recovery_cmd_results_catch_exception(self):
        @set_recovery_cmd_results
        def test_decorator(cmd):
            cmd.name = 'new name'
            cmd.error_type = 10
            raise Exception('running the cmd failed')

        recovery_info = RecoveryInfo('/tmp/datadir2', 7002, 2, None, None, None, None, None, '/tmp/progress_file2')
        test_cmd = FullRecovery('original name', recovery_info, True,None,None)
        test_decorator(test_cmd)
        self.assertEqual('new name', test_cmd.name)

        expected_stderr = '{"error_type": 10, "error_msg": "running the cmd failed", "dbid": 2, ' \
                          '"datadir": "/tmp/datadir2", "port": 7002, "progress_file": "/tmp/progress_file2"}'
        self._assert_cmd_failed(test_cmd, expected_stderr)

    def test_set_recovery_cmd_results_catch_exception_none_error_type(self):
        @set_recovery_cmd_results
        def test_decorator(cmd):
            cmd.name = 'new name'
            cmd.error_type = None
            raise Exception('running the cmd failed')
        recovery_info = RecoveryInfo('/tmp/datadir2', 7002, 2, None, None, None, None, None, '/tmp/progress_file2')
        test_cmd = FullRecovery('original name', recovery_info, True, None, None)
        test_decorator(test_cmd)
        self.assertEqual('new name', test_cmd.name)

        expected_stderr = '{"error_type": "default", "error_msg": "running the cmd failed", "dbid": 2, ' \
                          '"datadir": "/tmp/datadir2", "port": 7002, "progress_file": "/tmp/progress_file2"}'
        self._assert_cmd_failed(test_cmd, expected_stderr)
