from . import redirect_stderr
from mock import call, Mock, patch, ANY
import sys

from .gp_unittest import GpTestCase, FakeCursor
import gpsegrecovery
from gpsegrecovery import SegRecovery
import gppylib
from gppylib import gplog
from gppylib.gparray import Segment
from gppylib.recoveryinfo import RecoveryInfo, RecoveryErrorType


class IncrementalRecoveryTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.mock_logger = Mock()
        self.apply_patches([
            patch('gpsegrecovery.ModifyConfSetting', return_value=Mock()),
            patch('gpsegrecovery.start_segment', return_value=Mock()),
            patch('gppylib.commands.pg.PgRewind.__init__', return_value=None),
            patch('gppylib.commands.pg.PgRewind.run')
        ])
        self.mock_pgrewind_run = self.get_mock_from_apply_patch('run')
        self.mock_pgrewind_init = self.get_mock_from_apply_patch('__init__')
        self.mock_pgrewind_modifyconfsetting = self.get_mock_from_apply_patch('ModifyConfSetting')

        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              p.getSegmentDataDirectory(),
                                              False, False, '/tmp/test_progress_file')
        self.era = '1234_20211110'

        self.incremental_recovery_cmd = gpsegrecovery.IncrementalRecovery(
            name='test incremental recovery', recovery_info=self.seg_recovery_info,
            logger=self.mock_logger, era=self.era)

    def tearDown(self):
        super(IncrementalRecoveryTestCase, self).tearDown()

    def _assert_cmd_failed(self, expected_stderr):
        self.assertEqual(1, self.incremental_recovery_cmd.get_results().rc)
        self.assertEqual('', self.incremental_recovery_cmd.get_results().stdout)
        self.assertItemsEqual(expected_stderr, self.incremental_recovery_cmd.get_results().stderr)
        self.assertEqual(False, self.incremental_recovery_cmd.get_results().wasSuccessful())

    def test_incremental_run_passes(self):
        self.incremental_recovery_cmd.run()
        self.assertEqual(1, self.mock_pgrewind_init.call_count)
        expected_init_args = call('rewind dbid: 2', '/data/mirror0',
                                  'sdw1', 40000, '/tmp/test_progress_file')
        self.assertEqual(expected_init_args, self.mock_pgrewind_init.call_args)
        self.assertEqual(1, self.mock_pgrewind_run.call_count)
        self.assertEqual(1, self.mock_pgrewind_modifyconfsetting.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_pgrewind_run.call_args)
        logger_call_args = [call('Running pg_rewind with progress output temporarily in /tmp/test_progress_file'),
                            call('Successfully ran pg_rewind for dbid: 2'),
                            call("Updating /data/mirror0/postgresql.conf")]
        self.assertEqual(logger_call_args, self.mock_logger.info.call_args_list)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)

    def test_incremental_run_exception(self):
        self.mock_pgrewind_run.side_effect = [Exception('pg_rewind failed')]
        self.incremental_recovery_cmd.run()
        self.assertEqual(1, self.mock_pgrewind_init.call_count)
        expected_init_args = call('rewind dbid: 2', '/data/mirror0',
                                  'sdw1', 40000, '/tmp/test_progress_file')
        self.assertEqual(expected_init_args, self.mock_pgrewind_init.call_args)
        self.assertEqual(1, self.mock_pgrewind_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_pgrewind_run.call_args)
        self.assertEqual([call('Running pg_rewind with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('{"error_type": "incremental", "error_msg": "pg_rewind failed", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_logger_info_exception(self):
        self.mock_logger.info.side_effect = [Exception('logger exception')]
        self.incremental_recovery_cmd.run()
        self.assertEqual(0, self.mock_pgrewind_init.call_count)
        self.assertEqual(0, self.mock_pgrewind_run.call_count)
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('{"error_type": "default", "error_msg": "logger exception", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_incremental_start_segment_exception(self):
        gpsegrecovery.start_segment.side_effect = [Exception('pg_ctl start failed')]
        self.incremental_recovery_cmd.run()

        self.assertEqual(1, self.mock_pgrewind_init.call_count)
        self.assertEqual(1, self.mock_pgrewind_run.call_count)
        logger_call_args = [call('Running pg_rewind with progress output temporarily in /tmp/test_progress_file'),
                            call('Successfully ran pg_rewind for dbid: 2'),
                            call("Updating /data/mirror0/postgresql.conf")]
        self.assertEqual(logger_call_args, self.mock_logger.info.call_args_list)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)
        self._assert_cmd_failed('{"error_type": "start", "error_msg": "pg_ctl start failed", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_incremental_modify_conf_setting_exception(self):
        self.mock_pgrewind_modifyconfsetting.side_effect = [Exception('modify conf port failed'), Mock()]
        self.incremental_recovery_cmd.run()

        self.assertEqual(1, self.mock_pgrewind_init.call_count)
        self.assertEqual(1, self.mock_pgrewind_run.call_count)
        self.mock_pgrewind_modifyconfsetting.assert_called_once_with('Updating %s/postgresql.conf' % self.seg_recovery_info.target_datadir,
                                                                     "{}/{}".format(self.seg_recovery_info.target_datadir, 'postgresql.conf'),
                                                                     'port', self.seg_recovery_info.target_port, optType='number')
        self.assertEqual(1, self.mock_pgrewind_modifyconfsetting.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('{"error_type": "update", "error_msg": "modify conf port failed", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')



class FullRecoveryTestCase(GpTestCase):
    def setUp(self):
        # TODO should we mock the set_recovery_cmd_results decorator and not worry about
        # testing the command results in this test class
        self.maxDiff = None
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.apply_patches([
            patch('gpsegrecovery.ModifyConfSetting', return_value=Mock()),
            patch('gpsegrecovery.start_segment', return_value=Mock()),
            patch('gpsegrecovery.PgBaseBackup.__init__', return_value=None),
            patch('gpsegrecovery.PgBaseBackup.run')
        ])
        self.mock_pgbasebackup_run = self.get_mock_from_apply_patch('run')
        self.mock_pgbasebackup_init = self.get_mock_from_apply_patch('__init__')
        self.mock_pgbasebackup_modifyconfsetting = self.get_mock_from_apply_patch('ModifyConfSetting')

        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              p.getSegmentDataDirectory(),
                                              True, False, '/tmp/test_progress_file')
        self.era = '1234_20211110'
        self.full_recovery_cmd = gpsegrecovery.FullRecovery(
            name='test full recovery', recovery_info=self.seg_recovery_info,
            forceoverwrite=True, logger=self.mock_logger, era=self.era)

    def tearDown(self):
        super(FullRecoveryTestCase, self).tearDown()

    def _assert_basebackup_runs(self, expected_init_args):
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(expected_init_args, self.mock_pgbasebackup_init.call_args)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        self.assertEqual(1, self.mock_pgbasebackup_modifyconfsetting.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_pgbasebackup_run.call_args)
        expected_logger_info_args = [call('Running pg_basebackup with progress output temporarily in /tmp/test_progress_file'),
                                     call("Successfully ran pg_basebackup for dbid: 2"),
                                     call("Updating /data/mirror0/postgresql.conf")]
        self.assertEqual(expected_logger_info_args, self.mock_logger.info.call_args_list)

        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)

    def _assert_cmd_passed(self):
        self.assertEqual(0, self.full_recovery_cmd.get_results().rc)
        self.assertEqual('', self.full_recovery_cmd.get_results().stdout)
        self.assertEqual('', self.full_recovery_cmd.get_results().stderr)
        self.assertEqual(True, self.full_recovery_cmd.get_results().wasSuccessful())

    def _assert_cmd_failed(self, expected_stderr):
        self.assertEqual(1, self.full_recovery_cmd.get_results().rc)
        self.assertEqual('', self.full_recovery_cmd.get_results().stdout)
        self.assertItemsEqual(expected_stderr, self.full_recovery_cmd.get_results().stderr)
        self.assertEqual(False, self.full_recovery_cmd.get_results().wasSuccessful())

    def test_basebackup_run_passes(self):
        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=True, target_gp_dbid=2, progress_file='/tmp/test_progress_file')

        self._assert_basebackup_runs(expected_init_args1)
        self._assert_cmd_passed()

    def test_basebackup_run_no_forceoverwrite_passes(self):
        self.full_recovery_cmd.forceoverwrite = False

        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=False, target_gp_dbid=2, progress_file='/tmp/test_progress_file')
        self._assert_basebackup_runs(expected_init_args1)
        self._assert_cmd_passed()

    def test_basebackup_run_exception(self):
        self.mock_pgbasebackup_run.side_effect = [Exception('backup failed once')]

        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=True, target_gp_dbid=2, progress_file='/tmp/test_progress_file')
        self.assertEqual([expected_init_args1], self.mock_pgbasebackup_init.call_args_list)
        self.assertEqual([call(validateAfter=True)], self.mock_pgbasebackup_run.call_args_list)
        self.assertEqual([call('Running pg_basebackup with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('{"error_type": "full", "error_msg": "backup failed once", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_basebackup_run_no_forceoverwrite_run_exception(self):
        self.mock_pgbasebackup_run.side_effect = [Exception('backup failed once')]
        self.full_recovery_cmd.forceoverwrite = False

        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=False, target_gp_dbid=2, progress_file='/tmp/test_progress_file')
        self.assertEqual([expected_init_args1], self.mock_pgbasebackup_init.call_args_list)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        self.assertEqual([call(validateAfter=True)], self.mock_pgbasebackup_run.call_args_list)
        self.assertEqual([call('Running pg_basebackup with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('{"error_type": "full", "error_msg": "backup failed once", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_basebackup_init_exception(self):
        self.mock_pgbasebackup_init.side_effect = [Exception('backup init failed')]

        self.full_recovery_cmd.run()
        expected_init_args = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                  forceoverwrite=True, target_gp_dbid=2, progress_file='/tmp/test_progress_file')
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(expected_init_args, self.mock_pgbasebackup_init.call_args)
        self.assertEqual(0, self.mock_pgbasebackup_run.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self.assertEqual(0, self.mock_logger.exception.call_count)
        self._assert_cmd_failed('{"error_type": "full", "error_msg": "backup init failed", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_basebackup_start_segment_exception(self):
        gpsegrecovery.start_segment.side_effect = [Exception('pg_ctl start failed'), Mock()]

        self.full_recovery_cmd.run()
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)
        self._assert_cmd_failed('{"error_type": "start", "error_msg": "pg_ctl start failed", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

    def test_basebackup_modify_conf_setting_exception(self):
        self.mock_pgbasebackup_modifyconfsetting.side_effect = [Exception('modify conf port failed'), Mock()]
        self.full_recovery_cmd.run()

        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        self.mock_pgbasebackup_modifyconfsetting.assert_called_once_with('Updating %s/postgresql.conf' % self.seg_recovery_info.target_datadir,
                                                                          "{}/{}".format(self.seg_recovery_info.target_datadir, 'postgresql.conf'),
                                                                          'port', self.seg_recovery_info.target_port, optType='number')
        self.assertEqual(1, self.mock_pgbasebackup_modifyconfsetting.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('{"error_type": "update", "error_msg": "modify conf port failed", "dbid": 2, ' \
                                '"datadir": "/data/mirror0", "port": 50000, "progress_file": "/tmp/test_progress_file"}')

class SegRecoveryTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.full_r1 = RecoveryInfo('target_data_dir1', 5001, 1, 'source_hostname1',
                                    6001, 'source_datadair1', True, False, '/tmp/progress_file1')
        self.incr_r1 = RecoveryInfo('target_data_dir2', 5002, 2, 'source_hostname2',
                                    6002, 'source_datadir2', False, False, '/tmp/progress_file2')
        self.full_r2 = RecoveryInfo('target_data_dir3', 5003, 3, 'source_hostname3',
                                    6003, 'source_datadir3', True, False, '/tmp/progress_file3')
        self.incr_r2 = RecoveryInfo('target_data_dir4', 5004, 4, 'source_hostname4',
                                    6004, 'source_datadir4', False, False, '/tmp/progress_file4')
        self.diff_r1 = RecoveryInfo('target_data_dir5', 5005, 5, 'source_hostname5',
                                    6005, 'source_datadir5', False, True, '/tmp/progress_file5')
        self.diff_r2 = RecoveryInfo('target_data_dir6', 5006, 6, 'source_hostname6',
                                    6006, 'source_datadir6', False, True, '/tmp/progress_file6')
        self.era = '1234_2021110'

        self.apply_patches([
            patch('gpsegrecovery.SegmentStart.__init__', return_value=None),
            patch('gpsegrecovery.SegmentStart.run'),
        ])
        self.mock_segment_start_init = self.get_mock_from_apply_patch('__init__')

    def tearDown(self):
        super(SegRecoveryTestCase, self).tearDown()

    @patch('gppylib.commands.pg.PgRewind.__init__', return_value=None)
    @patch('gppylib.commands.pg.PgRewind.run')
    @patch('gpsegrecovery.PgBaseBackup.__init__', return_value=None)
    @patch('gpsegrecovery.PgBaseBackup.run')
    def test_complete_workflow(self, mock_pgbasebackup_run, mock_pgbasebackup_init, mock_pgrewind_run, mock_pgrewind_init):
        mix_confinfo = gppylib.recoveryinfo.serialize_list([
            self.full_r1, self.incr_r2])
        sys.argv = ['gpsegrecovery', '-l', '/tmp/logdir', '--era', '{}'.format(self.era), '-c {}'.format(mix_confinfo)]
        with redirect_stderr() as buf:
            with self.assertRaises(SystemExit) as ex:
                SegRecovery().main()
        self.assertEqual('', buf.getvalue().strip())
        self.assertEqual(0, ex.exception.code)
        self.assertEqual(1, mock_pgrewind_run.call_count)
        self.assertEqual(1, mock_pgrewind_init.call_count)
        self.assertEqual(1, mock_pgbasebackup_run.call_count)
        self.assertEqual(1, mock_pgbasebackup_init.call_count)
        self.assertRegexpMatches(gplog.get_logfile(), '/gpsegrecovery.py_\d+\.log')

    @patch('gppylib.commands.pg.PgRewind.__init__', return_value=None)
    @patch('gppylib.commands.pg.PgRewind.run')
    @patch('gpsegrecovery.PgBaseBackup.__init__', return_value=None)
    @patch('gpsegrecovery.PgBaseBackup.run')
    def test_complete_workflow_exception(self, mock_pgbasebackup_run, mock_pgbasebackup_init, mock_pgrewind_run,
                                         mock_pgrewind_init):
        mock_pgrewind_run.side_effect = [Exception('pg_rewind failed')]
        mock_pgbasebackup_run.side_effect = [Exception('pg_basebackup failed once')]
        mix_confinfo = gppylib.recoveryinfo.serialize_list([
            self.full_r1, self.incr_r2])
        sys.argv = ['gpsegrecovery', '-l', '/tmp/logdir', '--era={}'.format(self.era), '-c {}'.format(mix_confinfo)]
        with redirect_stderr() as buf:
            with self.assertRaises(SystemExit) as ex:
                SegRecovery().main()

        self.assertItemsEqual('[{"error_type": "incremental", "error_msg": "pg_rewind failed", "dbid": 4, "datadir": "target_data_dir4", '
                              '"port": 5004, "progress_file": "/tmp/progress_file4"} , '
                              '{"error_type": "full", "error_msg": "pg_basebackup failed once", "dbid": 1,'
                              '"datadir": "target_data_dir1", "port": 5001, "progress_file": "/tmp/progress_file1"}]',
                                buf.getvalue().strip())

        self.assertEqual(1, ex.exception.code)
        self.assertEqual(1, mock_pgrewind_run.call_count)
        self.assertEqual(1, mock_pgrewind_init.call_count)
        self.assertEqual(1, mock_pgbasebackup_run.call_count)
        self.assertEqual(1, mock_pgbasebackup_init.call_count)
        self.assertRegexpMatches(gplog.get_logfile(), '/gpsegrecovery.py_\d+\.log')

    @patch('recovery_base.gplog.setup_tool_logging')
    @patch('recovery_base.RecoveryBase.main')
    @patch('gpsegrecovery.SegRecovery.get_recovery_cmds')
    def test_get_recovery_cmds_is_called(self, mock_get_recovery_cmds, mock_recovery_base_main, mock_logger):
        mix_confinfo = gppylib.recoveryinfo.serialize_list([self.full_r1, self.incr_r2])
        sys.argv = ['gpsegrecovery', '-l', '/tmp/logdir', '--era={}'.format(self.era), '-f',
                    '-c {}'.format(mix_confinfo)]
        SegRecovery().main()
        mock_get_recovery_cmds.assert_called_once_with([self.full_r1, self.incr_r2], True, mock_logger.return_value,
                                                       self.era)
        mock_recovery_base_main.assert_called_once_with(mock_get_recovery_cmds.return_value)

    def _assert_validation_full_call(self, cmd, expected_recovery_info,
                                     expected_forceoverwrite=False):
        self.assertTrue(
            isinstance(cmd, gpsegrecovery.FullRecovery))
        self.assertIn('pg_basebackup', cmd.name)
        self.assertEqual(expected_recovery_info, cmd.recovery_info)
        self.assertEqual(expected_forceoverwrite, cmd.forceoverwrite)
        self.assertEqual(self.era, cmd.era)
        self.assertEqual(self.mock_logger, cmd.logger)

    def _assert_setup_incr_call(self, cmd, expected_recovery_info):
        self.assertTrue(
            isinstance(cmd, gpsegrecovery.IncrementalRecovery))
        self.assertIn('pg_rewind', cmd.name)
        self.assertEqual(expected_recovery_info, cmd.recovery_info)
        self.assertEqual(self.era, cmd.era)
        self.assertEqual(self.mock_logger, cmd.logger)

    def _assert_setup_diff_call(self, cmd, expected_recovery_info):
        self.assertTrue(
            isinstance(cmd, gpsegrecovery.DifferentialRecovery))
        self.assertIn('rsync', cmd.name)
        self.assertEqual(expected_recovery_info, cmd.recovery_info)
        self.assertEqual(self.era, cmd.era)
        self.assertEqual(self.mock_logger, cmd.logger)

    def test_empty_recovery_info_list(self):
        cmd_list = SegRecovery().get_recovery_cmds([], False, None, self.era)
        self.assertEqual([], cmd_list)

    def test_get_recovery_cmds_full_recoveryinfo(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.full_r1, self.full_r2], False, self.mock_logger, self.era)
        self._assert_validation_full_call(cmd_list[0], self.full_r1)
        self._assert_validation_full_call(cmd_list[1], self.full_r2)

    def test_get_recovery_cmds_incr_recoveryinfo(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.incr_r1, self.incr_r2], False, self.mock_logger, self.era)
        self._assert_setup_incr_call(cmd_list[0], self.incr_r1)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)

    def test_get_recovery_cmds_diff_recoveryinfo(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.diff_r1, self.diff_r2], False, self.mock_logger, self.era)
        self._assert_setup_diff_call(cmd_list[0], self.diff_r1)
        self._assert_setup_diff_call(cmd_list[1], self.diff_r2)

    def test_get_recovery_cmds_mix_recoveryinfo(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.full_r1, self.incr_r2, self.diff_r1], False, self.mock_logger, self.era)
        self._assert_validation_full_call(cmd_list[0], self.full_r1)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)
        self._assert_setup_diff_call(cmd_list[2], self.diff_r1)

    def test_get_recovery_cmds_mix_recoveryinfo_forceoverwrite(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.full_r1, self.incr_r2, self.diff_r1], True, self.mock_logger, self.era)
        self._assert_validation_full_call(cmd_list[0], self.full_r1,
                                          expected_forceoverwrite=True)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)
        self._assert_setup_diff_call(cmd_list[2], self.diff_r1)

    @patch('gpsegrecovery.SegmentStart.__init__', return_value=None)
    @patch('gpsegrecovery.SegmentStart.run')
    def test_start_segment_passes(self, mock_run, mock_init):
        gpsegrecovery.start_segment(self.full_r1, self.mock_logger, self.era)

        #TODO assert for args of this function
        mock_init.assert_called_once()
        self.assertEqual(1, self.mock_logger.info.call_count)
        mock_run.assert_called_once_with(validateAfter=True)


class DifferentialRecoveryClsTestCase(GpTestCase):
    def setUp(self):
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.apply_patches([
               patch('gppylib.commands.unix.Rsync.__init__', return_value=None),
               patch('gppylib.commands.unix.Rsync.run'),
               patch('gppylib.db.dbconn.connect', autospec=True)
        ])

        self.mock_rsync_init = self.get_mock_from_apply_patch('__init__')
        self.mock_rsync_run = self.get_mock_from_apply_patch('run')

        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              p.getSegmentDataDirectory(),
                                              False, True, '/tmp/test_progress_file')
        self.era = '1234_20211110'
        self.diff_recovery_cmd = gpsegrecovery.DifferentialRecovery(
            name='test differential recovery', recovery_info=self.seg_recovery_info,
            logger=self.mock_logger, era=self.era)

    @patch('gppylib.db.catalog.RemoteQueryCommand.run', side_effect=Exception())
    def test_pg_start_backup_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            self.diff_recovery_cmd.pg_start_backup()
        self.assertTrue('Failed to query pg_start_backup() for segment with host sdw1 and port 40000'
                        in str(ex.exception))

    @patch('gppylib.db.catalog.RemoteQueryCommand.__init__', return_value=None)
    @patch('gppylib.db.catalog.RemoteQueryCommand.run')
    def test_pg_start_backup_success(self, mock1, mock2):
        self.diff_recovery_cmd.pg_start_backup()
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Successfully ran pg_start_backup for segment on host sdw1, port 40000')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.catalog.RemoteQueryCommand.run', side_effect=Exception())
    def test_pg_stop_backup_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            self.diff_recovery_cmd.pg_stop_backup()
        self.assertTrue('Failed to query pg_stop_backup() for segment with host sdw1 and port 40000'
                        in str(ex.exception))

    @patch('gppylib.db.catalog.RemoteQueryCommand.__init__', return_value=None)
    @patch('gppylib.db.catalog.RemoteQueryCommand.run')
    def test_pg_stop_backup_success(self, mock1, mock2):
        self.diff_recovery_cmd.pg_stop_backup()
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Successfully ran pg_stop_backup for segment on host sdw1, port 40000')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.catalog.RemoteQueryCommand.get_results',
           return_value=[['/data/mytblspace1'], ['/data/mytblspace2']])
    def test_sync_tablespaces_outside_data_dir(self, mock):
        self.diff_recovery_cmd.sync_tablespaces()
        self.assertEqual(2, self.mock_rsync_init.call_count)
        self.assertEqual(2, self.mock_rsync_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_rsync_run.call_args)
        self.assertEqual([call('Syncing tablespaces of dbid 2 which are outside of data_dir')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.catalog.RemoteQueryCommand.get_results',
           return_value=[['/data/mirror0']])
    def test_sync_tablespaces_within_data_dir(self, mock):
        self.diff_recovery_cmd.sync_tablespaces()
        self.assertEqual(0, self.mock_rsync_init.call_count)
        self.assertEqual(0, self.mock_rsync_run.call_count)
        self.assertEqual([call('Syncing tablespaces of dbid 2 which are outside of data_dir')],
                         self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.catalog.RemoteQueryCommand.get_results',
           return_value=[['/data/mirror0'], ['/data/mytblspace1']])
    def test_sync_tablespaces_mix_data_dir(self, mock):
        self.diff_recovery_cmd.sync_tablespaces()
        self.assertEqual(1, self.mock_rsync_init.call_count)
        self.assertEqual(1, self.mock_rsync_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_rsync_run.call_args)
        self.assertEqual([call('Syncing tablespaces of dbid 2 which are outside of data_dir')],
                         self.mock_logger.debug.call_args_list)

    def test_sync_xlog_and_control_file_success(self):
        self.diff_recovery_cmd.sync_xlog_and_control_file()
        self.assertEqual(2, self.mock_rsync_init.call_count)
        self.assertEqual(2, self.mock_rsync_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_rsync_run.call_args)
        self.assertEqual([call('Syncing pg_xlog directory of dbid 2'),
                          call('Syncing pg_control file of dbid 2')],
                         self.mock_logger.debug.call_args_list)

    def test_sync_pg_data_success(self):
        self.diff_recovery_cmd.sync_pg_data()
        self.assertEqual(1, self.mock_rsync_init.call_count)
        self.assertEqual(1, self.mock_rsync_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_rsync_run.call_args)
        self.assertEqual([call('Syncing pg_data of dbid 2')],
                         self.mock_logger.debug.call_args_list)

    def tearDown(self):
        super(DifferentialRecoveryClsTestCase, self).tearDown()


class DifferentialRecoveryRunTestCase(GpTestCase):
    def setUp(self):
        # TODO should we mock the set_recovery_cmd_results decorator and not worry about
        # testing the command results in this test class
        self.maxDiff = None
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.apply_patches([
            patch('gpsegrecovery.DifferentialRecovery.sync_tablespaces', return_value = Mock()),
            patch('gpsegrecovery.DifferentialRecovery.pg_start_backup', return_value=Mock()),
            patch('gpsegrecovery.DifferentialRecovery.pg_stop_backup', return_value=Mock()),
            patch('gpsegrecovery.ModifyConfSetting', return_value=Mock()),
            patch('gpsegrecovery.start_segment', return_value=Mock()),
            patch('gppylib.commands.pg.PgReplicationSlot.slot_exists', return_value=Mock()),
            patch('gppylib.commands.pg.PgReplicationSlot.drop_slot', return_value=Mock()),
            patch('gppylib.commands.pg.PgReplicationSlot.create_slot', return_value=Mock()),
            patch('gppylib.commands.pg.PgBaseBackup.__init__', return_value=None),
            patch('gppylib.commands.pg.PgBaseBackup.run'),
            patch('gppylib.db.dbconn.connect', autospec=True),
            patch('gppylib.db.dbconn.execSQL', return_value=FakeCursor(my_list=[[True]])),
        ])

        self.mock_pgbasebackup_modifyconfsetting = self.get_mock_from_apply_patch('ModifyConfSetting')
        self.mock_sync_tablespaces = self.get_mock_from_apply_patch('sync_tablespaces')
        self.mock_pg_start_backup = self.get_mock_from_apply_patch('pg_start_backup')
        self.mock_pg_stop_backup = self.get_mock_from_apply_patch('pg_stop_backup')
        self.mock_pgbasebackup_init = self.get_mock_from_apply_patch('__init__')
        self.mock_pgbasebackup_run = self.get_mock_from_apply_patch('run')

        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              p.getSegmentDataDirectory(),
                                              False, True, '/tmp/test_progress_file')
        self.era = '1234_20211110'
        self.diff_recovery_cmd = gpsegrecovery.DifferentialRecovery(
            name='test differential recovery', recovery_info=self.seg_recovery_info,
            logger=self.mock_logger, era=self.era)

    def tearDown(self):
        super(DifferentialRecoveryRunTestCase, self).tearDown()

    def _assert_rsync_runs(self):
        self.assertEqual(3, self.mock_rsync_init.call_count)
        self.assertEqual(3, self.mock_rsync_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_rsync_run.call_args)
        expected_logger_info_args = [
            call('Running differential recovery with progress output temporarily in /tmp/test_progress_file'),
            call("Successfully ran differential recovery for dbid 2"),
            call("Updating /data/mirror0/postgresql.conf")]
        self.assertEqual(expected_logger_info_args, self.mock_logger.info.call_args_list)
        self.mock_pgbasebackup_modifyconfsetting.assert_called_once_with(
            'Updating %s/postgresql.conf' % self.seg_recovery_info.target_datadir,
            "{}/{}".format(self.seg_recovery_info.target_datadir, 'postgresql.conf'),
            'port', self.seg_recovery_info.target_port, optType='number')
        self.assertEqual(1, self.mock_pgbasebackup_modifyconfsetting.call_count)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)

    def _assert_basebackup_runs(self, expected_init_args):
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(expected_init_args, self.mock_pgbasebackup_init.call_args)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        self.assertEqual(1, self.mock_pgbasebackup_modifyconfsetting.call_count)
        self.mock_pgbasebackup_modifyconfsetting.assert_called_once_with(
            'Updating %s/postgresql.conf' % self.seg_recovery_info.target_datadir,
            "{}/{}".format(self.seg_recovery_info.target_datadir, 'postgresql.conf'),
            'port', self.seg_recovery_info.target_port, optType='number')
        self.assertEqual(call(validateAfter=True), self.mock_pgbasebackup_run.call_args)
        expected_logger_info_args = [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file'),
                                     call("Successfully ran differential recovery for dbid 2"),
                                     call("Updating /data/mirror0/postgresql.conf")]
        self.assertEqual(expected_logger_info_args, self.mock_logger.info.call_args_list)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)

    def _assert_cmd_passed(self):
        self.assertEqual(0, self.diff_recovery_cmd.get_results().rc)
        self.assertEqual('', self.diff_recovery_cmd.get_results().stdout)
        self.assertEqual('', self.diff_recovery_cmd.get_results().stderr)
        self.assertTrue(self.diff_recovery_cmd.get_results().wasSuccessful())

    def _assert_cmd_failed(self):
        self.assertEqual(1, self.diff_recovery_cmd.get_results().rc)
        self.assertEqual('', self.diff_recovery_cmd.get_results().stdout)
        self.assertEqual('', self.diff_recovery_cmd.get_results().stderr)
        self.assertFalse(self.diff_recovery_cmd.get_results().wasSuccessful())

    @patch('gpsegrecovery.DifferentialRecovery.write_conf_files', return_value=Mock())
    @patch('gppylib.commands.unix.Rsync.__init__', return_value=None)
    @patch('gppylib.commands.unix.Rsync.run')
    def test_rsync_run_passes(self, run, init, mock1):
        self.mock_rsync_run = run
        self.mock_rsync_init = init
        self.diff_recovery_cmd.run()
        self._assert_rsync_runs()
        self._assert_cmd_passed()

    @patch('gppylib.commands.unix.Rsync.__init__', return_value=None)
    @patch('gppylib.commands.unix.Rsync.run')
    def test_basebackup_run_passes(self, mock1, mock2):
        self.diff_recovery_cmd.run()
        expected_init_args = call("/data/mirror0", "sdw1", '40000', writeconffilesonly=True,
                                  target_gp_dbid=2, recovery_mode=False)
        self._assert_basebackup_runs(expected_init_args)
        self._assert_cmd_passed()

    @patch('gppylib.commands.pg.PgReplicationSlot.slot_exists', side_effect=Exception())
    def test_diff_recovery_slot_exists_exception(self, mock1):
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual([call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)

    @patch('gppylib.commands.pg.PgReplicationSlot.drop_slot', side_effect=Exception())
    def test_diff_recovery_drop_slot_exception(self, mock1):
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual([call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)

    def test_diff_recovery_pg_start_backup_exception(self):
        self.mock_pg_start_backup.side_effect = Exception()
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual([call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)

    @patch('gppylib.commands.pg.PgReplicationSlot.create_slot', side_effect=Exception())
    def test_diff_recovery_create_slot_exception(self, mock1):
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_pg_stop_backup.call_count)
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual([call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
                         self.mock_logger.info.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', side_effect=Exception())
    def test_diff_recovery_sync_pg_data_exception(self, mock1):
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_pg_stop_backup.call_count)
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(
            [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
            self.mock_logger.info.call_args_list)

    def test_diff_recovery_sync_tablespaces_exception(self):
        self.mock_sync_tablespaces.side_effect = Exception()
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_pg_stop_backup.call_count)
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Syncing pg_data of dbid 2')],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(
            [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
            self.mock_logger.info.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    def test_diff_recovery_pg_stop_backup_exception(self, mock1):
        self.mock_pg_stop_backup.side_effect = Exception()
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(
            [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
            self.mock_logger.info.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    @patch('gpsegrecovery.DifferentialRecovery.write_conf_files', side_effect=Exception())
    def test_diff_recovery_write_conf_files_exception(self, mock1, mock2):
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(
            [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
            self.mock_logger.info.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    @patch('gpsegrecovery.DifferentialRecovery.sync_xlog_and_control_file', side_effect=Exception())
    @patch('gpsegrecovery.DifferentialRecovery.write_conf_files', return_value=Mock())
    def test_diff_recovery_sync_xlog_and_control_file_exception(self, mock1, mock2,mock3):
        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_logger.info.call_count)
        self.assertEqual(
            [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file')],
            self.mock_logger.info.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    @patch('gpsegrecovery.DifferentialRecovery.sync_xlog_and_control_file', return_value=Mock())
    def test_diff_recovery_modify_conf_setting_exception(self,mock1,mock2):
        self.mock_pgbasebackup_modifyconfsetting.side_effect = [Exception('modify conf port failed'), Mock()]
        self.diff_recovery_cmd.run()

        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        self.mock_pgbasebackup_modifyconfsetting.assert_called_once_with('Updating %s/postgresql.conf' % self.seg_recovery_info.target_datadir,
                                                                          "{}/{}".format(self.seg_recovery_info.target_datadir, 'postgresql.conf'),
                                                                          'port', self.seg_recovery_info.target_port, optType='number')
        self.assertEqual(1, self.mock_pgbasebackup_modifyconfsetting.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self.assertEqual(2, self.mock_logger.debug.call_count)
        self.assertEqual([call('Writing recovery.conf and internal.auto.conf files for dbid 2'),
                          call('Running pg_basebackup to only write configuration files')],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual(3, self.mock_logger.info.call_count)
        self.assertEqual(
            [call('Running differential recovery with progress output temporarily in /tmp/test_progress_file'),
             call('Successfully ran differential recovery for dbid 2'),
             call('Updating /data/mirror0/postgresql.conf')],
            self.mock_logger.info.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    @patch('gpsegrecovery.DifferentialRecovery.sync_xlog_and_control_file', return_value=Mock())
    def test_diff_recovery_start_segment_exception(self,mock1,mock2):
        gpsegrecovery.start_segment.side_effect = [Exception('pg_ctl start failed'), Mock()]

        self.diff_recovery_cmd.run()
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    @patch('gpsegrecovery.DifferentialRecovery.sync_xlog_and_control_file', return_value=Mock())
    def test_diff_recovery_basebackup_run_one_exception(self,mock1,mock2):
        self.mock_pgbasebackup_run.side_effect = [Exception('backup failed once'), Mock()]

        self.diff_recovery_cmd.run()

        expected_init_args = call("/data/mirror0", "sdw1", '40000', writeconffilesonly=True,
                                  target_gp_dbid=2, recovery_mode=False)

        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual([expected_init_args], self.mock_pgbasebackup_init.call_args_list)
        self.assertEqual(1, self.mock_pgbasebackup_run.call_count)
        self.assertEqual([call(validateAfter=True)], self.mock_pgbasebackup_run.call_args_list)
        self.mock_logger.info.any_call('Running pg_basebackup failed: backup failed once')
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self.assertEqual(2, self.mock_logger.debug.call_count)
        self.assertEqual([call('Writing recovery.conf and internal.auto.conf files for dbid 2'),
                          call('Running pg_basebackup to only write configuration files')],
                         self.mock_logger.debug.call_args_list)

    @patch('gpsegrecovery.DifferentialRecovery.sync_pg_data', return_value=Mock())
    @patch('gpsegrecovery.DifferentialRecovery.sync_xlog_and_control_file', return_value=Mock())
    def test_diff_recovery_basebackup_init_exception(self,mock1,mock2):
        self.mock_pgbasebackup_init.side_effect = [Exception('backup init failed')]

        self.diff_recovery_cmd.run()
        expected_init_args = call("/data/mirror0", "sdw1", '40000', writeconffilesonly=True,
                                  target_gp_dbid=2, recovery_mode=False)
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(expected_init_args, self.mock_pgbasebackup_init.call_args)
        self.assertEqual(0, self.mock_pgbasebackup_run.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self.assertEqual(0, self.mock_logger.exception.call_count)
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Writing recovery.conf and internal.auto.conf files for dbid 2')],
                         self.mock_logger.debug.call_args_list)
