from . import redirect_stderr
from mock import call, Mock, patch, ANY
import sys

from .gp_unittest import GpTestCase
import gpsegrecovery
from gpsegrecovery import SegRecovery
import gppylib
from gppylib import gplog
from gppylib.gparray import Segment
from gppylib.recoveryinfo import RecoveryInfo


class IncrementalRecoveryTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.mock_logger = Mock()
        self.apply_patches([
            patch('gpsegrecovery.start_segment', return_value=Mock()),
            patch('gppylib.commands.pg.PgRewind.__init__', return_value=None),
            patch('gppylib.commands.pg.PgRewind.run')
        ])
        self.mock_pgrewind_run = self.get_mock_from_apply_patch('run')
        self.mock_pgrewind_init = self.get_mock_from_apply_patch('__init__')

        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              False, '/test_progress_file')
        self.era = '1234_20211110'

        self.incremental_recovery_cmd = gpsegrecovery.IncrementalRecovery(
            name='test incremental recovery', recovery_info=self.seg_recovery_info,
            logger=self.mock_logger, era=self.era)

    def tearDown(self):
        super(IncrementalRecoveryTestCase, self).tearDown()

    def test_incremental_run_passes(self):
        self.incremental_recovery_cmd.run()
        self.assertEqual(1, self.mock_pgrewind_init.call_count)
        expected_init_args = call('rewind dbid: 2', '/data/mirror0',
                                  'sdw1', 40000, '/test_progress_file')
        self.assertEqual(expected_init_args, self.mock_pgrewind_init.call_args)
        self.assertEqual(1, self.mock_pgrewind_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_pgrewind_run.call_args)
        logger_call_args = [call('Running pg_rewind with progress output temporarily in /test_progress_file'),
                            call('Successfully ran pg_rewind for dbid: 2')]
        self.assertEqual(logger_call_args, self.mock_logger.info.call_args_list)
        gpsegrecovery.start_segment.assert_called_once_with(self.seg_recovery_info, self.mock_logger, self.era)

    def test_incremental_run_exception(self):
        self.mock_pgrewind_run.side_effect = [Exception('pg_rewind failed')]
        self.incremental_recovery_cmd.run()
        self.assertEqual(1, self.mock_pgrewind_init.call_count)
        expected_init_args = call('rewind dbid: 2', '/data/mirror0',
                                  'sdw1', 40000, '/test_progress_file')
        self.assertEqual(expected_init_args, self.mock_pgrewind_init.call_args)
        self.assertEqual(1, self.mock_pgrewind_run.call_count)
        self.assertEqual(call(validateAfter=True), self.mock_pgrewind_run.call_args)
        self.assertEqual([call('Running pg_rewind with progress output temporarily in /test_progress_file')],
                         self.mock_logger.info.call_args_list)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)


class FullRecoveryTestCase(GpTestCase):
    def setUp(self):
        # TODO should we mock the set_cmd_results decorator and not worry about
        # testing the command results in this test class
        self.maxDiff = None
        self.mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
        self.apply_patches([
            patch('gpsegrecovery.start_segment', return_value=Mock()),
            patch('gpsegrecovery.PgBaseBackup.__init__', return_value=None),
            patch('gpsegrecovery.PgBaseBackup.run')
        ])
        self.mock_pgbasebackup_run = self.get_mock_from_apply_patch('run')
        self.mock_pgbasebackup_init = self.get_mock_from_apply_patch('__init__')

        p = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        m = Segment.initFromString("2|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.seg_recovery_info = RecoveryInfo(m.getSegmentDataDirectory(),
                                              m.getSegmentPort(),
                                              m.getSegmentDbId(),
                                              p.getSegmentHostName(),
                                              p.getSegmentPort(),
                                              True, '/test_progress_file')
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
        self.assertEqual(call(validateAfter=True), self.mock_pgbasebackup_run.call_args)
        expected_logger_info_args = [call('Running pg_basebackup with progress output temporarily in /test_progress_file'),
                                     call("Successfully ran pg_basebackup for dbid: 2")]
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
        self.assertEqual(expected_stderr, self.full_recovery_cmd.get_results().stderr)
        self.assertEqual(False, self.full_recovery_cmd.get_results().wasSuccessful())

    def test_basebackup_run_passes(self):
        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=True, target_gp_dbid=2, progress_file='/test_progress_file')

        self._assert_basebackup_runs(expected_init_args1)
        self._assert_cmd_passed()

    def test_basebackup_run_no_forceoverwrite_passes(self):
        self.full_recovery_cmd.forceoverwrite = False

        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=False, target_gp_dbid=2, progress_file='/test_progress_file')
        self._assert_basebackup_runs(expected_init_args1)
        self._assert_cmd_passed()

    def test_basebackup_run_exception(self):
        self.mock_pgbasebackup_run.side_effect = [Exception('backup failed once'), Mock()]

        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=True, target_gp_dbid=2, progress_file='/test_progress_file')
        self.assertEqual([expected_init_args1], self.mock_pgbasebackup_init.call_args_list)
        self.assertEqual([call(validateAfter=True)], self.mock_pgbasebackup_run.call_args_list)
        self.assertEqual([call('Running pg_basebackup with progress output temporarily in /test_progress_file')],
                         self.mock_logger.info.call_args_list)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('backup failed once')

    def test_basebackup_run_no_forceoverwrite_run_exception(self):
        self.mock_pgbasebackup_run.side_effect = [Exception('backup failed once'), Mock()]
        self.full_recovery_cmd.forceoverwrite = False

        self.full_recovery_cmd.run()

        expected_init_args1 = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                   forceoverwrite=False, target_gp_dbid=2, progress_file='/test_progress_file')
        self.assertEqual([expected_init_args1], self.mock_pgbasebackup_init.call_args_list)
        self.assertEqual([call(validateAfter=True)], self.mock_pgbasebackup_run.call_args_list)
        self.assertEqual([call('Running pg_basebackup with progress output temporarily in /test_progress_file')],
                         self.mock_logger.info.call_args_list)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self._assert_cmd_failed('backup failed once')

    def test_basebackup_init_exception(self):
        self.mock_pgbasebackup_init.side_effect = [Exception('backup init failed')]

        self.full_recovery_cmd.run()
        expected_init_args = call("/data/mirror0", "sdw1", '40000', replication_slot_name='internal_wal_replication_slot',
                                  forceoverwrite=True, target_gp_dbid=2, progress_file='/test_progress_file')
        self.assertEqual(1, self.mock_pgbasebackup_init.call_count)
        self.assertEqual(expected_init_args, self.mock_pgbasebackup_init.call_args)
        self.assertEqual(0, self.mock_pgbasebackup_run.call_count)
        self.assertEqual(0, gpsegrecovery.start_segment.call_count)
        self.assertEqual(0, self.mock_logger.exception.call_count)
        self._assert_cmd_failed('backup init failed')


class SegRecoveryTestCase(GpTestCase):
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
        mix_confinfo = gppylib.recoveryinfo.serialize_recovery_info_list([
            self.full_r1, self.incr_r2])
        sys.argv = ['gpsegrecovery', '-l', '/tmp/logdir', '--era', '{}'.format(self.era), '-c {}'.format(mix_confinfo)]
        with redirect_stderr() as buf:
            with self.assertRaises(SystemExit) as ex:
                seg_recovery = SegRecovery()
                seg_recovery.main()
        self.assertEqual('', buf.getvalue())
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
        mix_confinfo = gppylib.recoveryinfo.serialize_recovery_info_list([
            self.full_r1, self.incr_r2])
        sys.argv = ['gpsegrecovery', '-l', '/tmp/logdir', '--era={}'.format(self.era), '-c {}'.format(mix_confinfo)]
        with redirect_stderr() as buf:
            with self.assertRaises(SystemExit) as ex:
                seg_recovery = SegRecovery()
                seg_recovery.main()
        self.assertEqual('pg_rewind failed\n', buf.getvalue())
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
        mix_confinfo = gppylib.recoveryinfo.serialize_recovery_info_list([self.full_r1, self.incr_r2])
        sys.argv = ['gpsegrecovery', '-l', '/tmp/logdir', '--era={}'.format(self.era), '-f',
                    '-c {}'.format(mix_confinfo)]
        seg_recovery = SegRecovery()
        seg_recovery.main()
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

    def test_get_recovery_cmds_mix_recoveryinfo(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.full_r1, self.incr_r2], False, self.mock_logger, self.era)
        self._assert_validation_full_call(cmd_list[0], self.full_r1)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)

    def test_get_recovery_cmds_mix_recoveryinfo_forceoverwrite(self):
        cmd_list = SegRecovery().get_recovery_cmds([self.full_r1, self.incr_r2], True, self.mock_logger, self.era)
        self._assert_validation_full_call(cmd_list[0], self.full_r1,
                                          expected_forceoverwrite=True)
        self._assert_setup_incr_call(cmd_list[1], self.incr_r2)

    @patch('gpsegrecovery.SegmentStart.__init__', return_value=None)
    @patch('gpsegrecovery.SegmentStart.run')
    def test_start_segment_passes(self, mock_run, mock_init):
        gpsegrecovery.start_segment(self.full_r1, self.mock_logger, self.era)

        #TODO assert for args of this function
        mock_init.assert_called_once()
        self.assertEqual(1, self.mock_logger.info.call_count)
        mock_run.assert_called_once_with(validateAfter=True)
