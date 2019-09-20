import imp
import logging
import os
import signal
import sys
import time
import unittest

from mock import Mock, call, patch

from gppylib.gparray import Segment, GpArray, SegmentPair
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.commands import base
from gppylib.commands.base import Command, WorkerPool
from gppylib.commands.gp import GpSegStopCmd
from gppylib.mainUtils import ProgramArgumentValidationException

try:
    import subprocess32
    mock_subprocess_str='subprocess32.call'
except:
    mock_subprocess_str='subprocess.call'

class GpStop(GpTestCase):
    def setUp(self):
        # because gpstop does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpstop.py, this is equivalent to:
        #   import gpstop
        #   self.subject = gpstop
        gpstop_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpstop")
        self.subject = imp.load_source('gpstop', gpstop_file)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.mock_gp = Mock()
        self.mock_pgconf = Mock()
        self.mock_os = Mock()

        self.mock_conn = Mock()
        self.mock_catalog = Mock()
        self.mock_gperafile = Mock()
        self.mock_unix = Mock()
        self.gparray = self.createGpArrayWith4Primary4Mirrors()

        self.apply_patches([
            patch('gpstop.gp', return_value=self.mock_gp),
            patch.object(GpSegStopCmd, "__init__", return_value=None),
            patch('gpstop.pgconf', return_value=self.mock_pgconf),
            patch('gpstop.os', return_value=self.mock_os),
            patch('gpstop.dbconn.connect', return_value=self.mock_conn),
            patch('gpstop.catalog', return_value=self.mock_catalog),
            patch('gpstop.unix', return_value=self.mock_unix),
            patch('gpstop.GpEraFile', return_value=self.mock_gperafile),
            patch('gpstop.GpArray.initFromCatalog'),
            patch('gpstop.gphostcache.unix.Ping'),
            patch('gpstop.RemoteOperation'),
            patch('gpstop.base.WorkerPool'),
            patch('gpstop.socket.gethostname'),
            patch('gpstop.print_progress'),
        ])

        sys.argv = ["gpstop"]  # reset to relatively empty args list

        # TODO: We are not unit testing how we report the segment stops. We've currently mocked it out
        self.mock_workerpool = self.get_mock_from_apply_patch('WorkerPool')
        self.mock_GpSegStopCmdInit = self.get_mock_from_apply_patch('__init__')
        self.mock_gparray = self.get_mock_from_apply_patch('initFromCatalog')
        self.mock_gparray.return_value = self.gparray
        self.mock_socket = self.get_mock_from_apply_patch('gethostname')

    def tearDown(self):
        super(GpStop, self).tearDown()

    def createGpArrayWith4Primary4Mirrors(self):
        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")

        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw1|sdw1|40001|/data/primary1")
        self.primary2 = Segment.initFromString(
            "4|2|p|p|s|u|sdw2|sdw2|40002|/data/primary2")
        self.primary3 = Segment.initFromString(
            "5|3|p|p|s|u|sdw2|sdw2|40003|/data/primary3")

        self.mirror0 = Segment.initFromString(
            "6|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.mirror1 = Segment.initFromString(
            "7|1|m|m|s|u|sdw2|sdw2|50001|/data/mirror1")
        self.mirror2 = Segment.initFromString(
            "8|2|m|m|s|u|sdw1|sdw1|50002|/data/mirror2")
        self.mirror3 = Segment.initFromString(
            "9|3|m|m|s|u|sdw1|sdw1|50003|/data/mirror3")
        return GpArray([self.master, self.primary0, self.primary1, self.primary2, self.primary3, self.mirror0, self.mirror1, self.mirror2, self.mirror3])

    def get_info_messages(self):
        return [args[0][0] for args in self.subject.logger.info.call_args_list]

    def get_error_messages(self):
        return [args[0][0] for args in self.subject.logger.error.call_args_list]


    @patch('gpstop.userinput', return_value=Mock(spec=['ask_yesno']))
    def test_option_master_success_without_auto_accept(self, mock_userinput):
        sys.argv = ["gpstop", "-m"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        mock_userinput.ask_yesno.return_value = True
        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        self.assertEqual(mock_userinput.ask_yesno.call_count, 1)
        mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with master-only shutdown', 'N')

    @patch('gpstop.userinput', return_value=Mock(spec=['ask_yesno']))
    def test_option_master_success_with_auto_accept(self, mock_userinput):
        sys.argv = ["gpstop", "-m", "-a"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        mock_userinput.ask_yesno.return_value = True
        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        self.assertEqual(mock_userinput.ask_yesno.call_count, 0)

    def test_option_hostonly_succeeds(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        log_messages = self.get_info_messages()
        self.assertNotIn("sdw1   /data/mirror1    50001   u", log_messages)

        self.assertIn("Targeting dbid %s for shutdown" % [self.primary0.getSegmentDbId(),
                                                                   self.primary1.getSegmentDbId(),
                                                                   self.mirror2.getSegmentDbId(),
                                                                   self.mirror3.getSegmentDbId()], log_messages)
        self.assertIn("Successfully shutdown 4 of 8 segment instances ", log_messages)

    def test_host_missing_from_config(self):
        sys.argv = ["gpstop", "-a", "--host", "nothere"]
        host_names = self.gparray.getSegmentsByHostName(self.gparray.getDbList()).keys()

        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()
        gpstop = self.subject.GpStop.createProgram(options, args)
        with self.assertRaises(SystemExit) as cm:
            gpstop.run()
        self.assertEquals(cm.exception.code, 1)
        error_msgs = self.get_error_messages()
        self.assertIn("host 'nothere' is not found in gp_segment_configuration", error_msgs)
        self.assertIn("hosts in cluster config: %s" % host_names, error_msgs)

    @patch('gpstop.userinput', return_value=Mock(spec=['ask_yesno']))
    def test_happypath_in_interactive_mode(self, mock_userinput):
        sys.argv = ["gpstop", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        mock_userinput.ask_yesno.return_value = True
        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        log_messages = self.get_info_messages()

        # two calls per host, first for primaries then for mirrors
        self.assertEquals(2, self.mock_GpSegStopCmdInit.call_count)
        self.assertIn("Targeting dbid %s for shutdown" % [self.primary0.getSegmentDbId(),
                                                                   self.primary1.getSegmentDbId(),
                                                                   self.mirror2.getSegmentDbId(),
                                                                   self.mirror3.getSegmentDbId()], log_messages)

        # call_obj[0] returns all unnamed arguments -> ['arg1', 'arg2']
        # In this case, we have an object as an argument to pool.addCommand
        # call_obj[1] returns a dict for all named arguments -> {key='arg3', key2='arg4'}
        self.assertEquals(self.mock_GpSegStopCmdInit.call_args_list[0][1]['dbs'][0], self.primary0)
        self.assertEquals(self.mock_GpSegStopCmdInit.call_args_list[0][1]['dbs'][1], self.primary1)
        self.assertEquals(self.mock_GpSegStopCmdInit.call_args_list[1][1]['dbs'][0], self.mirror2)
        self.assertEquals(self.mock_GpSegStopCmdInit.call_args_list[1][1]['dbs'][1], self.mirror3)
        self.assertIn("   sdw1   /data/primary0   40000   u", log_messages)
        self.assertIn("   sdw1   /data/primary1   40001   u", log_messages)
        self.assertIn("   sdw1   /data/mirror2    50002   u", log_messages)
        self.assertIn("   sdw1   /data/mirror3    50003   u", log_messages)

        for line in log_messages:
            self.assertNotRegexpMatches(line, "sdw2")

        self.assertIn("Successfully shutdown 4 of 8 segment instances ", log_messages)

    def test_host_option_segment_in_change_tracking_mode_fails(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.primary0 = Segment.initFromString(
            "2|0|p|p|c|u|sdw1|sdw1|40000|/data/primary0")
        self.mirror0 = Segment.initFromString(
            "6|0|m|m|s|d|sdw2|sdw2|50000|/data/mirror0")
        self.mock_gparray.return_value = GpArray([self.master, self.primary0,
                                                  self.primary1, self.primary2,
                                                  self.primary3, self.mirror0,
                                                  self.mirror1, self.mirror2,
                                                  self.mirror3])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Segment '%s' not synchronized. Aborting." % self.primary0):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)

    def test_host_option_segment_in_resynchronizing_mode_fails(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.primary0 = Segment.initFromString(
            "2|0|p|p|r|u|sdw1|sdw1|40000|/data/primary0")
        self.mirror0 = Segment.initFromString(
            "6|0|m|m|r|u|sdw2|sdw2|50000|/data/mirror0")
        self.mock_gparray.return_value = GpArray([self.master, self.primary0,
                                                  self.primary1, self.primary2,
                                                  self.primary3, self.mirror0,
                                                  self.mirror1, self.mirror2,
                                                  self.mirror3])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Segment '%s' not synchronized. Aborting." % self.primary0):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)

    def test_host_option_segment_down_is_skipped_succeeds(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.primary0 = Segment.initFromString(
            "2|0|m|p|s|d|sdw1|sdw1|40000|/data/primary0")
        self.mirror0 = Segment.initFromString(
            "6|0|p|m|c|u|sdw2|sdw2|50000|/data/mirror0")
        self.mock_gparray.return_value = GpArray([self.master, self.primary0,
                                                  self.primary1, self.primary2,
                                                  self.primary3, self.mirror0,
                                                  self.mirror1, self.mirror2,
                                                  self.mirror3])

        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        log_messages = self.get_info_messages()

        self.assertEquals(2, self.mock_GpSegStopCmdInit.call_count)
        self.assertIn("Targeting dbid %s for shutdown" % [self.primary1.getSegmentDbId(),
                                                                   self.mirror2.getSegmentDbId(),
                                                                   self.mirror3.getSegmentDbId()], log_messages)
        self.assertIn("Successfully shutdown 3 of 8 segment instances ", log_messages)

    def test_host_option_segment_on_same_host_with_mirror_fails(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")

        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.mirror0 = Segment.initFromString(
            "3|0|m|m|s|u|sdw1|sdw1|50000|/data/mirror0")
        self.mock_gparray.return_value = GpArray([self.master, self.primary0, self.mirror0])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Segment host '%s' has both of corresponding primary '%s' and mirror '%s'. Aborting." % (self.primary0.getSegmentHostName(), self.primary0, self.mirror0)):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)

    def test_host_option_if_master_running_on_the_host_fails(self):
        sys.argv = ["gpstop", "-a", "--host", "mdw"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.mirror0 = Segment.initFromString(
            "3|0|m|m|s|u|sdw1|sdw1|50000|/data/mirror0")
        self.mock_gparray.return_value = GpArray([self.master, self.primary0, self.mirror0])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Specified host '%s' has the master or standby master on it. This node can only be stopped as part of a full-cluster gpstop, without '--host'." %
                                     self.master.getSegmentHostName()):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)

    def test_host_option_if_standby_running_on_the_host_fails(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw1"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        self.standby = Segment.initFromString(
            "2|-1|m|m|s|u|sdw1|sdw1|25432|/data/master")
        self.primary0 = Segment.initFromString(
            "3|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.mock_gparray.return_value = GpArray([self.master, self.standby, self.primary0, self.mirror0])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Specified host '%s' has the master or standby master on it. This node can only be stopped as part of a full-cluster gpstop, without '--host'." %
                                     self.standby.getSegmentHostName()):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)

    def test_host_option_if_no_mirrors_fails(self):
        sys.argv = ["gpstop", "-a", "--host", "sdw2"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|/data/master")
        self.standby = Segment.initFromString(
            "2|-1|m|m|s|u|sdw1|sdw1|25432|/data/master")
        self.primary0 = Segment.initFromString(
            "3|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "4|0|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        self.mock_gparray.return_value = GpArray([self.master, self.standby, self.primary0, self.primary1])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Cannot perform host-specific gpstop on a cluster without segment mirroring."):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)

    def test_host_option_with_master_option_fails(self):
        sys.argv = ["gpstop", "--host", "sdw1", "-m"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        with self.assertRaisesRegexp(ProgramArgumentValidationException, "Incompatible flags. Cannot mix '--host' "
                                                                         "option with '-m' for master-only."):
            self.subject.GpStop.createProgram(options, args)

    def test_host_option_with_restart_option_fails(self):
        sys.argv = ["gpstop", "--host", "sdw1", "-r"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        with self.assertRaisesRegexp(ProgramArgumentValidationException, "Incompatible flags. Cannot mix '--host' "
                                                                         "option with '-r' for restart."):
            self.subject.GpStop.createProgram(options, args)

    def test_reload_config_use_local_context(self):
        self.mock_socket.return_value = 'mdw'
        sys.argv = ["gpstop", "-u"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()
        self.mock_gparray.return_value = GpArray([self.master, self.primary0, self.primary1])
        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.gparray = GpArray([self.master, self.primary0, self.primary1])
        gpstop._sighup_cluster()
        self.assertEquals(3, self.mock_workerpool.addCommand.call_count)
        self.assertEquals(None, self.mock_workerpool.addCommand.call_args_list[0][0][0].remoteHost)
        self.assertEquals("sdw1", self.mock_workerpool.addCommand.call_args_list[1][0][0].remoteHost)
        self.assertEquals("sdw1", self.mock_workerpool.addCommand.call_args_list[2][0][0].remoteHost)

    def test_host_option_with_request_sighup_option_fails(self):
        sys.argv = ["gpstop", "--host", "sdw1", "-u"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        with self.assertRaisesRegexp(ProgramArgumentValidationException, "Incompatible flags. Cannot mix '--host' "
                                                                         "option with '-u' for config reload."):
            self.subject.GpStop.createProgram(options, args)

    def test_host_option_with_stop_standby_option_fails(self):
        sys.argv = ["gpstop", "--host", "sdw1", "-y"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        with self.assertRaisesRegexp(ProgramArgumentValidationException, "Incompatible flags. Cannot mix '--host' "
                                                                         "option with '-y' for skipping standby."):
            self.subject.GpStop.createProgram(options, args)

    @patch('gpstop.SegmentStop')
    def test_stop_standby_option(self, mock):
        self.standby = Segment.initFromString(
            "10|-1|m|m|s|u|smdw|smdw|5432|/data/standby_master")
        self.gparray = GpArray([self.master, self.primary0, self.primary1, self.primary2, self.primary3, self.mirror0, self.mirror1, self.mirror2, self.mirror3, self.standby])
        self.mock_gparray.return_value = self.gparray

        sys.argv = ["gpstop", "-a", "-y"]
        parser = self.subject.GpStop.createParser()
        options, args = parser.parse_args()

        gpstop = self.subject.GpStop.createProgram(options, args)
        gpstop.run()
        assert not mock.called
        log_message = self.get_info_messages()
        self.assertTrue("Stopping master standby host smdw mode=fast" not in log_message)
        self.assertTrue("No standby master host configured" not in log_message)


# Perform an 'import gpstop', as above.
_gpstop_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpstop")
gpstop = imp.load_source('gpstop', _gpstop_file)


class GpStopPrintProgressTestCase(unittest.TestCase):

    def setUp(self):
        self.logger = Mock(spec=logging.Logger)
        self.pool = WorkerPool(numWorkers=1, logger=self.logger)

    def tearDown(self):
        self.pool.haltWork()

    def test_print_progress_prints_once_with_completed_pool(self):
        self.pool.addCommand(Mock(spec=Command))
        self.pool.addCommand(Mock(spec=Command))
        self.pool.join()

        gpstop.print_progress(self.pool)
        self.logger.info.assert_called_once_with('100.00% of jobs completed')

    def test_print_progress_prints_once_with_empty_pool(self):
        gpstop.print_progress(self.pool)
        self.logger.info.assert_called_once_with('0.00% of jobs completed')

    def test_print_progress_prints_intermediate_progress(self):
        duration = 0.01

        cmd = Mock(spec=Command)
        def wait_for_duration():
            time.sleep(duration)
        cmd.run.side_effect = wait_for_duration

        self.pool.addCommand(Mock(spec=Command))
        self.pool.addCommand(cmd)

        # We run a command for ten milliseconds, printing progress every
        # millisecond, so at some point we should transition from 50% to 100%.
        gpstop.print_progress(self.pool, interval=(duration / 10))
        self.logger.info.assert_has_calls([
            call('50.00% of jobs completed'),
            call('100.00% of jobs completed'),
        ])


@patch('gppylib.db.dbconn.connect')
@patch(mock_subprocess_str)
class GpStopSmartModeTestCase(unittest.TestCase):
    def setUp(self):
        gpstop.logger = Mock(logging.Logger)
        self.gpstop = gpstop.GpStop('smart')

        # _SigIntHandler requires an ignored SIGINT to start with.
        self.orig_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)

    def tearDown(self):
        # Restore the original SIGINT handler even on test failure.
        signal.signal(signal.SIGINT, self.orig_handler)

    def _setup_subprocess(self, subprocess_call):
        self.stop_retval = 0
        self.status_callback = lambda: gpstop.PG_CTL_STATUS_STOPPED

        def _call(args, **kwargs):
            if args[0] != 'pg_ctl':
                self.fail('Expected only pg_ctl calls to be made during subprocess.call().')

            if args[1] == 'stop':
                return self.stop_retval
            elif args[1] == 'status':
                return self.status_callback()

            self.fail('Unimplemented pg_ctl command: {}'.format(args))

        subprocess_call.side_effect = _call

    def test_stop_master_smart_issues_pg_ctl_stop(self, subprocess_call, dbconn_connect):
        self._setup_subprocess(subprocess_call)

        self.gpstop.master_datadir = 'datadir'
        self.gpstop._stop_master_smart()
        subprocess_call.assert_any_call(['pg_ctl', 'stop', '-W', '-m', 'smart', '-D', 'datadir'])

    def test_stop_master_smart_raises_exception_if_stop_fails(self, subprocess_call, dbconn_connect):
        self._setup_subprocess(subprocess_call)
        self.stop_retval = 1

        with self.assertRaises(Exception):
            self.gpstop._stop_master_smart()

    def test_stop_master_smart_calls_pg_ctl_status_until_server_stops(self, subprocess_call, dbconn_connect):
        self._setup_subprocess(subprocess_call)
        self.gpstop.conn = dbconn_connect

        self.i = 0
        def _status():
            """
            This implementation of pg_ctl status will only show that the server
            has been stopped after the second call.
            """
            self.i += 1
            if self.i < 3:
                return gpstop.PG_CTL_STATUS_RUNNING
            return gpstop.PG_CTL_STATUS_STOPPED
        self.status_callback = _status

        self.gpstop._stop_master_smart()
        self.assertEqual(self.i, 3, "expected _stop_master_smart to return after the third call to pg_ctl status")

    def test_stop_master_smart_raises_exception_if_status_fails(self, subprocess_call, dbconn_connect):
        self._setup_subprocess(subprocess_call)
        self.status_callback = lambda: 1 # indicate general failure

        with self.assertRaises(Exception):
            self.gpstop._stop_master_smart()

    def test_SigIntHandler_must_have_SIGINT_ignored_on_entry(self, subprocess_call, dbconn_connect):
        handler = lambda num, frame: None
        orig_handler = signal.signal(signal.SIGINT, handler)

        try:
            with self.assertRaises(Exception):
                with gpstop.GpStop._SigIntHandler() as handler:
                    pass

            self.assertEqual(handler, signal.getsignal(signal.SIGINT))

        finally:
            # Restore the original signal handler even on test failure.
            signal.signal(signal.SIGINT, orig_handler)

    def test_SigIntHandler_catches_SIGINT_when_enabled(self, subprocess_call, dbconn_connect):
        with gpstop.GpStop._SigIntHandler() as handler:
            self.assertFalse(handler.interrupted)
            handler.enable()
            os.kill(os.getpid(), signal.SIGINT)
            self.assertTrue(handler.interrupted)

        # Ensure the context manager puts its previous handler back.
        self.assertEqual(signal.SIG_IGN, signal.getsignal(signal.SIGINT))

    def test_SigIntHandler_ignores_SIGINT_when_disabled(self, subprocess_call, dbconn_connect):
        with gpstop.GpStop._SigIntHandler() as handler:
            self.assertFalse(handler.interrupted)
            handler.disable()
            os.kill(os.getpid(), signal.SIGINT)
            self.assertFalse(handler.interrupted)

        # Ensure the context manager puts its previous handler back.
        self.assertEqual(signal.SIG_IGN, signal.getsignal(signal.SIGINT))


if __name__ == '__main__':
    run_tests()
