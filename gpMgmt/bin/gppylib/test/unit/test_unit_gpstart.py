import imp
import os
import sys

from mock import Mock, patch, call

from gppylib.gparray import Segment, GpArray
from gppylib.operations.startSegments import StartSegmentsResult
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.commands import gp
from gppylib.commands.base import ExecutionError
from gppylib.mainUtils import ExceptionNoStackTraceNeeded, UserAbortedException


class GpStart(GpTestCase):
    def setUp(self):
        # because gpstart does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpstart.py, this is equivalent to:
        #   import gpstart
        #   self.subject = gpstart
        gpstart_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpstart")
        self.subject = imp.load_source('gpstart', gpstart_file)
        self.subject.logger = Mock(
            spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal', 'warning_to_file_only'])

        self.os_environ = dict(COORDINATOR_DATA_DIRECTORY='/tmp/cdd', GPHOME='/tmp/gphome', GP_MGMT_PROCESS_COUNT=1,
                               LANGUAGE=None)
        self.gparray = self._createGpArrayWith2Primary2Mirrors()
        self.segments_by_content_id = GpArray.getSegmentsByContentId(self.gparray.getSegDbList())

        start_result = StartSegmentsResult()
        start_result.addSuccess(self.primary0)

        self.apply_patches([
            patch('os.getenv', side_effect=self._get_env),
            patch('gpstart.os.path.exists'),
            patch('gpstart.gp'),
            patch('gpstart.pgconf'),
            patch('gpstart.unix'),
            patch('gpstart.dbconn.DbURL'),
            patch('gpstart.dbconn.connect'),
            patch('gpstart.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gpstart.GpArray.getSegmentsByContentId', return_value=self.segments_by_content_id),
            patch('gpstart.GpArray.getSegmentsGroupedByValue',
                  side_effect=[{2: self.primary0, 3: self.primary1}, [], []]),
            patch('gpstart.GpEraFile'),
            patch('gpstart.userinput'),
            patch('gpstart.HeapChecksum'),
            patch('gpstart.log_to_file_only'),
            patch("gpstart.StartSegmentsOperation"),
            patch("gpstart.base.WorkerPool"),
            patch("gpstart.gp.CoordinatorStart.local"),
            patch("gpstart.pg.DbStatus.local"),
            patch("gpstart.TableLogger"),
        ])

        self.mock_start_result = self.get_mock_from_apply_patch('StartSegmentsOperation')
        self.mock_start_result.return_value.startSegments.return_value.getSuccessfulSegments.return_value = start_result.getSuccessfulSegments()

        self.mock_os_path_exists = self.get_mock_from_apply_patch('exists')
        self.mock_gp = self.get_mock_from_apply_patch('gp')
        self.mock_pgconf = self.get_mock_from_apply_patch('pgconf')
        self.mock_userinput = self.get_mock_from_apply_patch('userinput')
        self.mock_heap_checksum = self.get_mock_from_apply_patch('HeapChecksum')
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = True
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = ([], [], None)
        self.mock_pgconf.readfile.return_value = Mock()
        self.mock_gplog_log_to_file_only = self.get_mock_from_apply_patch("log_to_file_only")

        self.mock_gp.get_coordinatordatadir.return_value = 'masterdatadir'
        self.mock_gp.GpCatVersion.local.return_value = 1
        self.mock_gp.GpCatVersionDirectory.local.return_value = 1
        self.mock_gp.DEFAULT_GPSTART_NUM_WORKERS = gp.DEFAULT_GPSTART_NUM_WORKERS
        sys.argv = ["gpstart"]  # reset to relatively empty args list

    def tearDown(self):
        super(GpStart, self).tearDown()

    def setup_gpstart(self):
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)
        return gpstart

    def test_option_coordinator_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-m"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_new_option_coordinator_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-c"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_all_short_option_coordinator_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-c", "-m"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_all_options_coordinator_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-c", "-m", '--master_only', '--coordinator_only']
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_option_coordinator_success_with_auto_accept(self):
        sys.argv = ["gpstart", "-m", "-a"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 0)
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_new_option_coordinator_success_with_auto_accept(self):
        sys.argv = ["gpstart", "-c", "-a"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 0)
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_option_coordinator_exits_with_user_abort(self):
        sys.argv = ["gpstart", "-m"]
        self.mock_userinput.ask_yesno.return_value = False
        self.subject.unix.PgPortIsActive.local.return_value = False
        return_code = 4
        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        with self.assertRaises(UserAbortedException):
            return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.assertEqual(return_code, 4)

    def test_option_coordinator_restricted_success_with_auto_accept(self):
        sys.argv = ["gpstart", "-m", "-R", "-a"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        self.mock_pgconf.readfile.return_value.int.return_value = 99  # mock the port value

        gpstart = self.setup_gpstart()
        gpstart.coordinator_datadir = "/data/coordinator"

        return_code = gpstart.run()

        expected_args = call('Coordinator in utility mode with restricted set to True', gpstart.coordinator_datadir, 99, None, wrapper=None, wrapper_args=None,
                             specialMode=None, restrictedMode=True, timeout=600, utilityMode=True, max_connections=99)

        self.assertEqual([expected_args], self.subject.gp.CoordinatorStart.call_args_list) # assert that the CoordinatorStart function was called with the right arguments
        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 0)
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin and RESTRICTED mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_option_coordinator_restricted_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-m", "-R"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        self.mock_pgconf.readfile.return_value.int.return_value = 99  # mock the port value

        gpstart = self.setup_gpstart()
        gpstart.coordinator_datadir = "/data/coordinator"

        return_code = gpstart.run()

        expected_args = call('Coordinator in utility mode with restricted set to True', gpstart.coordinator_datadir, 99,
                             None, wrapper=None, wrapper_args=None,
                             specialMode=None, restrictedMode=True, timeout=600, utilityMode=True, max_connections=99)

        self.assertEqual([expected_args], self.subject.gp.CoordinatorStart.call_args_list) # assert that the CoordinatorStart function was called with the right arguments
        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin and RESTRICTED mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')
        self.assertEqual(return_code, 0)

    def test_gpstart_success_without_auto_accept(self):
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with Greenplum instance startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Database successfully started')
        self.assertEqual(return_code, 0)

    def test_gpstart_success_with_auto_accept(self):
        sys.argv = ["gpstart", "-a"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 0)
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Database successfully started')
        self.assertEqual(return_code, 0)

    def test_gpstart_exits_with_user_abort(self):
        self.mock_userinput.ask_yesno.return_value = False
        self.subject.unix.PgPortIsActive.local.return_value = False
        return_code = 4
        self.mock_os_path_exists.side_effect = os_exists_check

        gpstart = self.setup_gpstart()
        with self.assertRaises(UserAbortedException):
            return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with Greenplum instance startup', 'N')
        self.assertEqual(return_code, 4)

    def test_output_to_stdout_and_log_for_coordinator_only_happens_before_heap_checksum(self):
        sys.argv = ["gpstart", "-m"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        gpstart = self.setup_gpstart()

        return_code = gpstart.run()

        self.assertEqual(return_code, 0)
        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')

        self.assertEqual(self.mock_gplog_log_to_file_only.call_count, 0)

    def test_output_to_stdout_and_log_for_new_coordinator_only_happens_before_heap_checksum(self):
        sys.argv = ["gpstart", "-c"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        gpstart = self.setup_gpstart()

        return_code = gpstart.run()

        self.assertEqual(return_code, 0)
        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with coordinator-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Coordinator instance in admin mode')
        self.subject.logger.info.assert_any_call('Coordinator Started...')

        self.assertEqual(self.mock_gplog_log_to_file_only.call_count, 0)

    def test_skip_checksum_validation_succeeds(self):
        sys.argv = ["gpstart", "-a", "--skip-heap-checksum-validation"]
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        gpstart = self.setup_gpstart()

        return_code = gpstart.run()

        self.assertEqual(return_code, 0)
        messages = [msg[0][0] for msg in self.subject.logger.info.call_args_list]
        self.assertNotIn('Heap checksum setting is consistent across the cluster', messages)
        self.subject.logger.warning.assert_any_call('Because of --skip-heap-checksum-validation, '
                                                    'the GUC for data_checksums '
                                                    'will not be checked between coordinator and segments')

    def test_log_when_heap_checksum_validation_fails(self):
        sys.argv = ["gpstart", "-a", "-S"]
        self.mock_os_path_exists.side_effect = os_exists_check
        self.mock_heap_checksum.return_value.get_coordinator_value.return_value = 1
        start_failure = StartSegmentsResult()
        start_failure.addFailure(self.mirror1, "fictitious reason", gp.SEGSTART_ERROR_CHECKSUM_MISMATCH)
        self.mock_start_result.return_value.startSegments.return_value.getFailedSegmentObjs.return_value = start_failure.getFailedSegmentObjs()

        gpstart = self.setup_gpstart()

        return_code = gpstart.run()
        self.assertEqual(return_code, 1)
        messages = [msg[0][0] for msg in self.subject.logger.info.call_args_list]
        self.assertIn("DBID:5  FAILED  host:'sdw1' datadir:'/data/mirror1' with reason:'fictitious reason'", messages)

    def test_standby_startup_skipped(self):
        sys.argv = ["gpstart", "-a", "-y"]

        gpstart = self.setup_gpstart()

        return_value = gpstart._start_standby()
        self.assertFalse(return_value)
        messages = [msg[0][0] for msg in self.subject.logger.info.call_args_list]
        self.assertIn("No standby coordinator configured.  skipping...", messages)

    def test_prepare_segment_start_returns_up_and_down_segments(self):
        # Boilerplate: create a gpstart object
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args([])
        gpstart = self.subject.GpStart.createProgram(options, args)

        # Up segments
        coordinator = Segment.initFromString("1|-1|p|p|n|u|cdw|cdw|5432|/data/coordinator")
        primary1 = Segment.initFromString("3|1|p|p|n|u|sdw2|sdw2|40001|/data/primary1")
        mirror0 = Segment.initFromString("4|0|m|m|n|u|sdw2|sdw2|50000|/data/mirror0")

        # Down segments
        primary0 = Segment.initFromString("2|0|p|p|n|d|sdw1|sdw1|40000|/data/primary0")
        mirror1 = Segment.initFromString("5|1|m|m|n|d|sdw1|sdw1|50001|/data/mirror1")
        standby = Segment.initFromString("6|-1|m|m|n|d|sdw3|sdw3|5433|/data/standby")

        gpstart.gparray = GpArray([coordinator, primary0, primary1, mirror0, mirror1, standby])

        up, down = gpstart._prepare_segment_start()

        # The coordinator and standby should not be accounted for in these lists.
        self.assertCountEqual(up, [primary1, mirror0])
        self.assertCountEqual(down, [primary0, mirror1])

    @patch("gppylib.commands.pg.PgControlData.run")
    @patch("gppylib.commands.pg.PgControlData.get_value", return_value="2")
    def test_fetch_tli_returns_TimeLineID_when_standby_is_accessible(self, mock1, mock2):
        gpstart = self.setup_gpstart()

        self.assertEqual(gpstart.fetch_tli("", "foo"), 2)

    @patch("gpstart.GpStart.fetch_tli", autospec=True)
    def test_standby_activated_returns_false_when_primary_tli_is_before_standby_tli(self, mock_fetch_tli):
        def mock_fetch_tli_func(self, data_dir, remote_host=None):
            if "coordinator" in data_dir:
                return 3
            if "standby" in data_dir:
                return 2
            return 1

        mock_fetch_tli.side_effect = mock_fetch_tli_func

        gpstart = self.setup_gpstart()
        gpstart.coordinator_datadir = "/data/coordinator"

        coordinator = Segment.initFromString("1|-1|p|p|n|u|cdw|cdw|5432|/data/coordinator")
        standby = Segment.initFromString("6|-1|m|m|n|d|sdw3|sdw3|5433|/data/standby")
        gpstart.gparray = GpArray([coordinator, standby])

        self.assertFalse(gpstart._standby_activated())

    @patch("gpstart.GpStart.fetch_tli", autospec=True)
    def test_standby_activated_returns_true_when_standby_tli_is_before_primary_tli(self, mock_fetch_tli):
        def mock_fetch_tli_func(self, data_dir, remote_host=None):
            if "coordinator" in data_dir:
                return 1
            if "standby" in data_dir:
                return 2
            return 3

        mock_fetch_tli.side_effect = mock_fetch_tli_func

        gpstart = self.setup_gpstart()
        gpstart.coordinator_datadir = "/data/coordinator"

        coordinator = Segment.initFromString("1|-1|p|p|n|u|cdw|cdw|5432|/data/coordinator")
        standby = Segment.initFromString("6|-1|m|m|n|d|sdw3|sdw3|5433|/data/standby")
        gpstart.gparray = GpArray([coordinator, standby])

        self.assertTrue(gpstart._standby_activated())

    @patch("gpstart.GpStart.fetch_tli", autospec=True)
    def test_standby_activated_raises_StandbyUnreachable_exception_when_fetching_standby_tli_fails(self, mock_fetch_tli):
        def mock_fetch_tli_func(self, data_dir, remote_host=None):
            if "standby" in data_dir:
                raise ExecutionError("oops", None)
            return 10

        mock_fetch_tli.side_effect = mock_fetch_tli_func

        gpstart = self.setup_gpstart()
        gpstart.coordinator_datadir = "/data/coordinator"

        coordinator = Segment.initFromString("1|-1|p|p|n|u|cdw|cdw|5432|/data/coordinator")
        standby = Segment.initFromString("6|-1|m|m|n|d|sdw3|sdw3|5433|/data/standby")
        gpstart.gparray = GpArray([coordinator, standby])

        with self.assertRaises(gpstart.StandbyUnreachable):
            gpstart._standby_activated()

    @patch("gpstart.gp.GpStop")
    @patch("gpstart.GpStart._standby_activated", return_value=False)
    def test_check_standby_returns_when_standby_is_not_activated(self, mock_standby_activated, mock_gp_stop):
        gpstart = self.setup_gpstart()
        gpstart.check_standby()
        self.assertFalse(mock_gp_stop.called)

    @patch("gpstart.gp.GpStop")
    @patch("gpstart.GpStart._standby_activated", return_value=True)
    def test_check_standby_stops_coordinator_and_raises_an_exception_when_standby_is_activated(self, mock_standby_activated, mock_gp_stop):
        gpstart = self.setup_gpstart()
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            gpstart.check_standby()
        self.assertTrue(mock_gp_stop.return_value.run.called)

    @patch("gpstart.GpStart.shutdown_coordinator_only")
    @patch("gpstart.gp.GpStop")
    @patch("gpstart.GpStart._standby_activated")
    def test_check_standby_logs_warning_and_returns_when_standby_is_unreachable_and_user_proceeds(self, mock_standby_activated, mock_gp_stop, mock_shutdown_coordinator):
        gpstart = self.setup_gpstart()

        mock_standby_activated.side_effect = gpstart.StandbyUnreachable()
        gpstart.interactive = True
        self.mock_userinput.ask_yesno.return_value = True

        gpstart.check_standby()
        self.subject.logger.warning.assert_any_call(StringContains("Standby host is unreachable, cannot determine whether the standby is currently acting as the coordinator"))
        self.assertFalse(mock_shutdown_coordinator.called)
        self.assertFalse(mock_gp_stop.called)

    @patch("gpstart.GpStart.shutdown_coordinator_only")
    @patch("gpstart.gp.GpStop")
    @patch("gpstart.GpStart._standby_activated")
    def test_check_standby_logs_warning_and_stops_coordinator_and_raises_exception_when_standby_is_unreachable_and_user_does_not_proceeed(self, mock_standby_activated, mock_gp_stop, mock_shutdown_coordinator):
        gpstart = self.setup_gpstart()

        mock_standby_activated.side_effect = gpstart.StandbyUnreachable()
        gpstart.interactive = True
        self.mock_userinput.ask_yesno.return_value = False

        with self.assertRaises(UserAbortedException):
            gpstart.check_standby()
        self.subject.logger.warning.assert_any_call(StringContains("Standby host is unreachable, cannot determine whether the standby is currently acting as the coordinator"))
        self.assertTrue(mock_shutdown_coordinator.called)
        self.assertFalse(mock_gp_stop.called)

    @patch("gpstart.GpStart.shutdown_coordinator_only")
    @patch("gpstart.gp.GpStop")
    @patch("gpstart.GpStart._standby_activated")
    def test_check_standby_logs_warning_and_stops_coordinator_and_raises_exception_in_non_interactive_mode_and_standby_is_unreachable(self, mock_standby_activated, mock_gp_stop, mock_shutdown_coordinator):
        gpstart = self.setup_gpstart()

        mock_standby_activated.side_effect = gpstart.StandbyUnreachable()
        gpstart.interactive = False

        with self.assertRaises(UserAbortedException):
            gpstart.check_standby()
        self.subject.logger.warning.assert_any_call(StringContains("Standby host is unreachable, cannot determine whether the standby is currently acting as the coordinator"))
        self.subject.logger.warning.assert_any_call("Non interactive mode detected. Not starting the cluster. Start the cluster in interactive mode.")
        self.assertTrue(mock_shutdown_coordinator.called)
        self.assertFalse(mock_gp_stop.called)

    def _createGpArrayWith2Primary2Mirrors(self):
        self.coordinator = Segment.initFromString(
            "1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        self.mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
        self.standby = Segment.initFromString(
            "6|-1|m|m|s|u|sdw3|sdw3|5433|/data/standby")
        return GpArray([self.coordinator, self.primary0, self.primary1, self.mirror0, self.mirror1])

    def _get_env(self, arg):
        if arg not in self.os_environ:
            return None
        return self.os_environ[arg]


def os_exists_check(arg):
    # Skip file related checks
    if 'log' in arg:
        return True
    elif 'postmaster.pid' in arg or '.s.PGSQL' in arg:
        return False
    return False


class StringContains(str):
    def __eq__(self, other):
        return self in other

if __name__ == '__main__':
    run_tests()
