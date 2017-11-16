import imp
import os
import sys

from gparray import GpDB, GpArray, Segment
from mock import Mock, patch
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.commands.gp import GpSegStopCmd


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
        ])

        sys.argv = ["gpstop"]  # reset to relatively empty args list

        # TODO: We are not unit testing how we report the segment stops. We've currently mocked it out
        self.mock_workerpool = self.get_mock_from_apply_patch('WorkerPool')
        self.mock_GpSegStopCmdInit = self.get_mock_from_apply_patch('__init__')
        self.mock_gparray = self.get_mock_from_apply_patch('initFromCatalog')
        self.mock_gparray.return_value = self.gparray

    def tearDown(self):
        super(GpStop, self).tearDown()

    def createGpArrayWith4Primary4Mirrors(self):
        self.master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")

        self.primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.primary1 = GpDB.initFromString(
            "3|1|p|p|s|u|sdw1|sdw1|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        self.primary2 = GpDB.initFromString(
            "4|2|p|p|s|u|sdw2|sdw2|40002|41002|/data/primary2||/data/primary2/base/10899,/data/primary2/base/1,/data/primary2/base/10898,/data/primary2/base/25780,/data/primary2/base/34782")
        self.primary3 = GpDB.initFromString(
            "5|3|p|p|s|u|sdw2|sdw2|40003|41003|/data/primary3||/data/primary3/base/10899,/data/primary3/base/1,/data/primary3/base/10898,/data/primary3/base/25780,/data/primary3/base/34782")

        self.mirror0 = GpDB.initFromString(
            "6|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        self.mirror1 = GpDB.initFromString(
            "7|1|m|m|s|u|sdw2|sdw2|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        self.mirror2 = GpDB.initFromString(
            "8|2|m|m|s|u|sdw1|sdw1|50002|51002|/data/mirror2||/data/mirror2/base/10899,/data/mirror2/base/1,/data/mirror2/base/10898,/data/mirror2/base/25780,/data/mirror2/base/34782")
        self.mirror3 = GpDB.initFromString(
            "9|3|m|m|s|u|sdw1|sdw1|50003|51003|/data/mirror3||/data/mirror3/base/10899,/data/mirror3/base/1,/data/mirror3/base/10898,/data/mirror3/base/25780,/data/mirror3/base/34782")
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
        # In this case, we have an object as an argument to poo.addCommand
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

        self.primary0 = GpDB.initFromString(
            "2|0|p|p|c|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.mirror0 = GpDB.initFromString(
            "6|0|m|m|s|d|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
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

        self.primary0 = GpDB.initFromString(
            "2|0|p|p|r|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.mirror0 = GpDB.initFromString(
            "6|0|m|m|r|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
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

        self.primary0 = GpDB.initFromString(
            "2|0|m|p|s|d|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.mirror0 = GpDB.initFromString(
            "6|0|p|m|c|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
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

        self.master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")

        self.primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.mirror0 = GpDB.initFromString(
            "3|0|m|m|s|u|sdw1|sdw1|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        self.mock_gparray.return_value = GpArray([self.master, self.primary0, self.mirror0])

        gpstop = self.subject.GpStop.createProgram(options, args)

        with self.assertRaisesRegexp(Exception,"Segment host '%s' has both of corresponding primary '%s' and mirror '%s'. Aborting." % (self.primary0.getSegmentHostName(), self.primary0, self.mirror0)):
            gpstop.run()
        self.assertEquals(0, self.mock_GpSegStopCmdInit.call_count)


if __name__ == '__main__':
    run_tests()
