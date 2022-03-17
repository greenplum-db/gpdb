import io
from contextlib import redirect_stderr
from mock import call, Mock, patch, ANY
import os
import sys
import tempfile

from .gp_unittest import GpTestCase
import gpsegsetuprecovery
from gpsegsetuprecovery import SegSetupRecovery
import gppylib
from gppylib import gplog
from gppylib.gparray import Segment
from gppylib.recoveryinfo import RecoveryInfo
from gppylib.commands.base import CommandResult, Command

class ValidationForFullRecoveryTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              True, '/tmp/test_progress_file')

        self.validation_recovery_cmd = gpsegsetuprecovery.ValidationForFullRecovery(
            name='test validation for full recovery', recovery_info=self.seg_recovery_info,
            forceoverwrite=True, logger=self.mock_logger)

    def tearDown(self):
        super(ValidationForFullRecoveryTestCase, self).tearDown()

    def _remove_dir_and_upper_dir_if_exists(self, tmp_dir):
        if os.path.exists(tmp_dir):
            os.rmdir(tmp_dir)
        if os.path.exists(os.path.dirname(tmp_dir)):
            os.rmdir(os.path.dirname(tmp_dir))

    def _assert_passed(self):
        self.assertEqual(0, self.validation_recovery_cmd.get_results().rc)
        self.assertEqual('', self.validation_recovery_cmd.get_results().stdout)
        self.assertEqual('', self.validation_recovery_cmd.get_results().stderr)
        self.assertEqual(True, self.validation_recovery_cmd.get_results().wasSuccessful())
        self.mock_logger.info.assert_called_with("Validation successful for segment with dbid: 2")

    def _assert_failed(self, expected_error):
        self.assertEqual(1, self.validation_recovery_cmd.get_results().rc)
        self.assertEqual('', self.validation_recovery_cmd.get_results().stdout)
        self.assertEqual(expected_error, self.validation_recovery_cmd.get_results().stderr)
        self.assertEqual(False, self.validation_recovery_cmd.get_results().wasSuccessful())

    def test_forceoverwrite_True(self):
        self.validation_recovery_cmd.run()
        self._assert_passed()

    def test_no_forceoverwrite_dir_exists(self):
        with tempfile.TemporaryDirectory() as d:
            self.seg_recovery_info.target_datadir = d
            self.validation_recovery_cmd.forceoverwrite = False

            self.validation_recovery_cmd.run()
        self._assert_passed()

    def test_no_forceoverwrite_only_upper_dir_exists(self):
        with tempfile.TemporaryDirectory() as d:
            tmp_dir = os.path.join(d, 'test_data_dir')
            os.makedirs(tmp_dir)
            os.rmdir(tmp_dir)
            self.seg_recovery_info.target_datadir = tmp_dir
            self.validation_recovery_cmd.forceoverwrite = False

            self.validation_recovery_cmd.run()
        self._assert_passed()

    def test_no_forceoverwrite_dir_doesnt_exist(self):
        tmp_dir = '/tmp/nonexistent_dir/test_datadir'
        self._remove_dir_and_upper_dir_if_exists(tmp_dir)
        self.assertFalse(os.path.exists(tmp_dir))
        self.seg_recovery_info.target_datadir = tmp_dir
        self.validation_recovery_cmd.forceoverwrite = False
        try:
            self.validation_recovery_cmd.run()
            self.assertTrue(os.path.exists(tmp_dir))
            self.assertEqual(0o700, os.stat(tmp_dir).st_mode & 0o777)
            self._assert_passed()
        finally:
            self._remove_dir_and_upper_dir_if_exists(tmp_dir)

    def test_validation_only_no_forceoverwrite_dir_doesnt_exist(self):
        tmp_dir = '/tmp/nonexistent_dir/test_datadir'
        self._remove_dir_and_upper_dir_if_exists(tmp_dir)
        self.assertFalse(os.path.exists(tmp_dir))
        try:
            self.seg_recovery_info.target_datadir = tmp_dir
            self.validation_recovery_cmd.forceoverwrite = False

            self.validation_recovery_cmd.run()
            self.assertTrue(os.path.exists(tmp_dir))
            self.assertEqual(0o700, os.stat(tmp_dir).st_mode & 0o777)
            self._assert_passed()
        finally:
            self._remove_dir_and_upper_dir_if_exists(tmp_dir)

    def test_validation_only_no_forceoverwrite_dir_exists(self):
        with tempfile.TemporaryDirectory() as d:
            self.seg_recovery_info.target_datadir = d
            self.validation_recovery_cmd.forceoverwrite = False

            self.validation_recovery_cmd.run()
        self._assert_passed()

    def test_no_forceoverwrite_dir_exists_not_empty(self):
        with tempfile.TemporaryDirectory() as d:
            # We pass the upper directory so that the lower directory isn't empty (temp = a/b/c , we pass a/b)
            self.seg_recovery_info.target_datadir = os.path.dirname(d)
            self.validation_recovery_cmd.forceoverwrite = False

            self.validation_recovery_cmd.run()

            error_str = "for segment with port 50000: Segment directory '{}' exists but is not empty!".format(os.path.dirname(d))
            expected_error = '{{"error_type": "validation", "error_msg": "{}", "dbid": 2, "datadir": "{}", "port": 50000, ' \
                             '"progress_file": "/tmp/test_progress_file"}}'.format(error_str, os.path.dirname(d))
            self._assert_failed(expected_error)

    @patch('gpsegsetuprecovery.os.makedirs' , side_effect=Exception('mkdirs failed'))
    def test_no_forceoverwrite_mkdir_exception(self, mock_os_mkdir):
        tmp_dir = '/tmp/nonexistent_dir2/test_datadir'
        self._remove_dir_and_upper_dir_if_exists(tmp_dir)
        self.assertFalse(os.path.exists(tmp_dir))
        try:
            self.seg_recovery_info.target_datadir = tmp_dir
            self.validation_recovery_cmd.forceoverwrite = False
            self.validation_recovery_cmd.run()
        finally:
            self._remove_dir_and_upper_dir_if_exists(tmp_dir)

        self._assert_failed('{"error_type": "validation", "error_msg": "mkdirs failed", "dbid": 2, '
                            '"datadir": "/tmp/nonexistent_dir2/test_datadir", "port": 50000, '
                            '"progress_file": "/tmp/test_progress_file"}')


class SetupForIncrementalRecoveryTestCase(GpTestCase):
    def setUp(self):
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.mock_conn_val = Mock()
        self.mock_dburl_val = Mock()
        self.apply_patches([patch('gpsegsetuprecovery.dbconn.connect', return_value=self.mock_conn_val),
                            patch('gpsegsetuprecovery.dbconn.DbURL', return_value=self.mock_dburl_val),
                            patch('gpsegsetuprecovery.dbconn.execSQL')])
        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              True, '/tmp/test_progress_file')

        self.setup_for_incremental_recovery_cmd = gpsegsetuprecovery.SetupForIncrementalRecovery(
            name='setup for incremental recovery', recovery_info=self.seg_recovery_info, logger=self.mock_logger)

    def tearDown(self):
        super(SetupForIncrementalRecoveryTestCase, self).tearDown()

    def _assert_cmd_passed(self):
        self.assertEqual(0, self.setup_for_incremental_recovery_cmd.get_results().rc)
        self.assertEqual('', self.setup_for_incremental_recovery_cmd.get_results().stdout)
        self.assertEqual('', self.setup_for_incremental_recovery_cmd.get_results().stderr)
        self.assertEqual(True, self.setup_for_incremental_recovery_cmd.get_results().wasSuccessful())

    def _assert_cmd_failed(self, expected_stderr):
        self.assertEqual(1, self.setup_for_incremental_recovery_cmd.get_results().rc)
        self.assertEqual('', self.setup_for_incremental_recovery_cmd.get_results().stdout)
        self.assertTrue(expected_stderr in self.setup_for_incremental_recovery_cmd.get_results().stderr)
        self.assertEqual(False, self.setup_for_incremental_recovery_cmd.get_results().wasSuccessful())

    def _assert_checkpoint_query(self):
        gpsegsetuprecovery.dbconn.DbURL.assert_called_once_with(hostname='sdw1', port=40000, dbname='template1')
        gpsegsetuprecovery.dbconn.connect.assert_called_once_with(self.mock_dburl_val, utility=True)
        gpsegsetuprecovery.dbconn.execSQL.assert_called_once_with(self.mock_conn_val, "CHECKPOINT")
        self.assertEqual(1, self.mock_conn_val.close.call_count)

    def test_setup_pid_does_not_exist_passes(self):
        self.setup_for_incremental_recovery_cmd.run()
        self._assert_checkpoint_query()
        self._assert_cmd_passed()

    def test_setup_pid_exist_passes(self):
        with tempfile.TemporaryDirectory() as d:
            self.seg_recovery_info.target_datadir = d
            f = open("{}/postmaster.pid".format(d), 'w')
            f.write('1111')
            f.close()
            self.assertTrue(os.path.exists("{}/postmaster.pid".format(d)))
            self.setup_for_incremental_recovery_cmd.run()
            self.assertFalse(os.path.exists("{}/postmaster.pid".format(d)))
        self._assert_checkpoint_query()
        self._assert_cmd_passed()

    @patch('gpsegsetuprecovery.Command', return_value=Command('rc1_cmd', 'echo 1 | grep 2'))
    def test_remove_pid_failed(self, mock_cmd):
        self.setup_for_incremental_recovery_cmd.run()
        self._assert_checkpoint_query()
        self._assert_cmd_failed("Failed while trying to remove postmaster.pid.")


class SegSetupRecoveryTestCase(GpTestCase):
    def setUp(self):
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.full_r1 = RecoveryInfo('target_data_dir1', 5001, 1, 'source_hostname1',
                                    6001, True, '/tmp/progress_file1')
        self.incr_r1 = RecoveryInfo('target_data_dir2', 5002, 2, 'source_hostname2',
                                    6002, False, '/tmp/progress_file2')
        self.full_r2 = RecoveryInfo('target_data_dir3', 5003, 3, 'source_hostname3',
                                    6003, True, '/tmp/progress_file3')
        self.incr_r2 = RecoveryInfo('target_data_dir4', 5004, 4, 'source_hostname4',
                                    6004, False, '/tmp/progress_file4')

    def tearDown(self):
        super(SegSetupRecoveryTestCase, self).tearDown()

    def _assert_validation_full_call(self, cmd, expected_recovery_info, expected_forceoverwrite=False):
        self.assertTrue(
            isinstance(cmd, gpsegsetuprecovery.ValidationForFullRecovery))
        self.assertIn('pg_basebackup', cmd.name)
        self.assertEqual(expected_recovery_info, cmd.recovery_info)
        self.assertEqual(expected_forceoverwrite, cmd.forceoverwrite)
        self.assertEqual(self.mock_logger, cmd.logger)

    def _assert_setup_incr_call(self, cmd, expected_recovery_info):
        self.assertTrue(
            isinstance(cmd, gpsegsetuprecovery.SetupForIncrementalRecovery))
        self.assertIn('pg_rewind', cmd.name)
        self.assertEqual(expected_recovery_info, cmd.recovery_info)
        self.assertEqual(self.mock_logger, cmd.logger)

    @patch('gpsegsetuprecovery.ValidationForFullRecovery.validate_failover_data_directory')
    @patch('gpsegsetuprecovery.dbconn.connect')
    @patch('gpsegsetuprecovery.dbconn.DbURL')
    @patch('gpsegsetuprecovery.dbconn.execSQL')
    def test_complete_workflow(self, mock_execsql, mock_dburl, mock_connect, mock_validate_datadir):
        mock_connect.return_value = Mock()
        mock_dburl.return_value = Mock()
        buf = io.StringIO()
        with redirect_stderr(buf):
            with self.assertRaises(SystemExit) as ex:
                mix_confinfo = gppylib.recoveryinfo.serialize_list([self.full_r1, self.incr_r2])
                sys.argv = ['gpsegsetuprecovery', '-l', '/tmp/logdir', '--era', '1234_2021',
                            '-c {}'.format(mix_confinfo)]
                SegSetupRecovery().main()
        self.assertEqual('', buf.getvalue().strip())
        self.assertEqual(0, ex.exception.code)
        mock_validate_datadir.assert_called_once()
        mock_dburl.assert_called_once()
        mock_connect.assert_called_once()
        mock_execsql.assert_called_once()
        #TODO use regex pattern
        self.assertRegex(gplog.get_logfile(), '/gpsegsetuprecovery.py_\d+\.log')

    @patch('gpsegsetuprecovery.ValidationForFullRecovery.validate_failover_data_directory')
    @patch('gpsegsetuprecovery.dbconn.connect')
    @patch('gpsegsetuprecovery.dbconn.DbURL')
    @patch('gpsegsetuprecovery.dbconn.execSQL')
    def test_complete_workflow_exception(self, mock_execsql, mock_dburl, mock_connect, mock_validate_datadir):
        mock_connect.side_effect = [Exception('connect failed')]
        mock_dburl.return_value = Mock()
        buf = io.StringIO()
        with redirect_stderr(buf):
            with self.assertRaises(SystemExit) as ex:
                mix_confinfo = gppylib.recoveryinfo.serialize_list([self.full_r1, self.incr_r2])
                sys.argv = ['gpsegsetuprecovery', '-l', '/tmp/logdir', '--era', '1234_2021',
                            '-c {}'.format(mix_confinfo)]
                SegSetupRecovery().main()

        self.assertEqual('[{"error_type": "validation", "error_msg": "connect failed", "dbid": 4, "datadir": "target_data_dir4", '
                         '"port": 5004, "progress_file": "/tmp/progress_file4"}]',
                         buf.getvalue().strip())

        self.assertEqual(1, ex.exception.code)
        mock_validate_datadir.assert_called_once()
        mock_dburl.assert_called_once()
        mock_connect.assert_called_once()
        self.assertRegex(gplog.get_logfile(), '/gpsegsetuprecovery.py_\d+\.log')


    @patch('recovery_base.gplog.setup_tool_logging')
    @patch('recovery_base.RecoveryBase.main')
    @patch('gpsegsetuprecovery.SegSetupRecovery.get_setup_cmds')
    def test_get_recovery_cmds_is_called(self, mock_get_setup_cmds, mock_recovery_base_main, mock_logger):
        mix_confinfo = gppylib.recoveryinfo.serialize_list([self.full_r1, self.incr_r2])
        sys.argv = ['gpsegsetuprecovery', '-l', '/tmp/logdir', '-f', '-c {}'.format(mix_confinfo)]
        SegSetupRecovery().main()
        mock_get_setup_cmds.assert_called_once_with([self.full_r1, self.incr_r2], True, mock_logger.return_value)
        mock_recovery_base_main.assert_called_once_with(mock_get_setup_cmds.return_value)

    def test_empty_recovery_info_list(self):
        cmd_list = SegSetupRecovery().get_setup_cmds([], False, None)
        self.assertEqual([], cmd_list)

    def test_get_setup_cmds_full_recoveryinfo(self):
        cmd_list = SegSetupRecovery().get_setup_cmds([
            self.full_r1, self.full_r2], False, self.mock_logger)
        self._assert_validation_full_call(cmd_list[0], self.full_r1)
        self._assert_validation_full_call(cmd_list[1], self.full_r2)

    def test_get_setup_cmds_incr_recoveryinfo(self):
        cmd_list = SegSetupRecovery().get_setup_cmds([
            self.incr_r1, self.incr_r2], False, self.mock_logger)
        self._assert_setup_incr_call(cmd_list[0], self.incr_r1)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)

    def test_get_setup_cmds_mix_recoveryinfo(self):
        cmd_list = SegSetupRecovery().get_setup_cmds([
            self.full_r1, self.incr_r2], False, self.mock_logger)
        self._assert_validation_full_call(cmd_list[0], self.full_r1)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)

    def test_get_setup_cmds_mix_recoveryinfo_forceoverwrite(self):
        cmd_list = SegSetupRecovery().get_setup_cmds([
            self.full_r1, self.incr_r2], True, self.mock_logger)
        self._assert_validation_full_call(cmd_list[0], self.full_r1, expected_forceoverwrite=True)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)

