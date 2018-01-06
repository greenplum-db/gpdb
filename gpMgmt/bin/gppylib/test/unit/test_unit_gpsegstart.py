import sys, os
import gpsegstart
from mock import Mock, patch
from gppylib.test.unit.gp_unittest import GpTestCase
from gppylib.commands.base import CommandResult
from gppylib.commands.pg import PgControlData
from gppylib.commands import gp


class GpSegStart(GpTestCase):
    def setUp(self):
        self.subject = gpsegstart
        self.subject.logger = Mock(
            spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal', 'warning_to_file_only'])
        local_version = gp.GpVersion.local('local GP software version check', gp.get_gphome())
        self.failing_data_dir = "/doesnt/exist/datadirs/dbfast_mirror1/demoDataDir0"
        self.args_list = ["gpsegstart.py", "-V", local_version,
                          "-n",  "3",  "--era", "5c105ee373d42194_180105120208", "-t", "600",  "-p", "pickled transition data",
                          "-D", "2|0|p|p|s|u|aspen|aspen|25432|25438|/doesnt/exist/datadirs/dbfast1/demoDataDir0||",
                          "-D", "5|0|m|m|s|u|aspen|aspen|25435|25441|" + self.failing_data_dir + "||",
                          "-D", "3|1|p|p|s|u|aspen|aspen|25433|25439|/doesnt/exist/datadirs/dbfast2/demoDataDir1||",
                          "-D", "6|1|m|m|s|u|aspen|aspen|25436|25442|/doesnt/exist/datadirs/dbfast_mirror2/demoDataDir1||",
                          "-D", "4|2|p|p|s|u|aspen|aspen|25434|25440|/doesnt/exist/datadirs/dbfast3/demoDataDir2||",
                          "-D", "7|2|m|m|s|u|aspen|aspen|25437|25443|/doesnt/exist/datadirs/dbfast_mirror3/demoDataDir2||",

                          ]
        self.apply_patches([
            patch('os.path.isdir'),
            patch('os.path.exists'),
            patch('os.kill'),
            patch('gpsegstart.gp.recovery_startup', return_value=None),
            patch('gpsegstart.base64.urlsafe_b64decode'),
            patch('gpsegstart.pickle.loads', return_value="random string"),
            patch('gpsegstart.gp.SendFilerepTransitionMessage.buildTransitionMessageCommand'),
            patch('gpsegstart.base.WorkerPool'),
            patch('gpsegstart.gp.read_postmaster_pidfile', return_value=111),
            patch('gpsegstart.PgControlData.run')
            ])

        self.mock_workerpool = self.get_mock_from_apply_patch('WorkerPool')

    def tearDown(self):
        super(GpSegStart, self).tearDown()

    def test_startSegments(self):
        sys.argv = self.args_list

        parser = self.subject.GpSegStart.createParser()
        options, args = parser.parse_args()

        gpsegstart = self.subject.GpSegStart.createProgram(options, args)
        exitCode = gpsegstart.run()

        self.assertEqual(exitCode, 0)
        for result in gpsegstart.overall_status.results:
            self.assertTrue(result.reasoncode == gp.SEGSTART_SUCCESS)

    @patch.object(PgControlData, "get_results", return_value=CommandResult(0, '/tmp/f1', '', True, False))
    @patch.object(PgControlData, "get_value", return_value="1")
    def test_startSegments_when_checksums_match(self, mock1, mock2):
        self.args_list.append("--master-checksum-version")
        self.args_list.append("1")
        sys.argv = self.args_list

        parser = self.subject.GpSegStart.createParser()
        options, args = parser.parse_args()

        gpsegstart = self.subject.GpSegStart.createProgram(options, args)
        exitCode = gpsegstart.run()

        self.assertEqual(exitCode, 0)
        for result in gpsegstart.overall_status.results:
            self.assertTrue(result.reasoncode == gp.SEGSTART_SUCCESS)

    @patch.object(PgControlData, "get_results", return_value=CommandResult(0, '/tmp/f1', '', True, False))
    @patch.object(PgControlData, "get_value", return_value="1")
    def test_startSegments_when_checksums_mismatch(self, mock1, mock2):
        self.args_list.append("--master-checksum-version")
        self.args_list.append("0")
        sys.argv = self.args_list

        parser = self.subject.GpSegStart.createParser()
        options, args = parser.parse_args()

        gpsegstart = self.subject.GpSegStart.createProgram(options, args)
        exitCode = gpsegstart.run()

        self.assertEqual(exitCode, 1)
        for result in gpsegstart.overall_status.results:
            self.assertTrue(result.reasoncode == gp.SEGSTART_ERROR_CHECKSUM_MISMATCH)

    @patch.object(PgControlData, "get_results", return_value=CommandResult(1, '/tmp/f1', '', True, False))
    def test_startSegments_when_pg_controldata_failed(self, mock1):
        self.args_list.append("--master-checksum-version")
        self.args_list.append("1")
        sys.argv = self.args_list

        parser = self.subject.GpSegStart.createParser()
        options, args = parser.parse_args()

        gpsegstart = self.subject.GpSegStart.createProgram(options, args)
        exitCode = gpsegstart.run()

        self.assertEqual(exitCode, 1)
        for result in gpsegstart.overall_status.results:
            self.assertTrue(result.reasoncode == gp.SEGSTART_ERROR_PG_CONTROLDATA_FAILED)

    @patch.object(PgControlData, "get_results", return_value=CommandResult(0, '/tmp/f1', '', True, False))
    @patch.object(PgControlData, "get_value", return_value="1")
    @patch.object(gp.SegmentStart, "get_results", return_value=CommandResult(1, '/tmp/f1', '', True, False))
    def test_startSegments_when_pg_ctl_failed(self, mock1, mock2, mock_get_results):
        self.args_list.append("--master-checksum-version")
        self.args_list.append("1")
        sys.argv = self.args_list

        mock_get_results.segment = Mock()
        mock_get_results.segment.getSegmentDataDirectory.return_value = self.failing_data_dir

        parser = self.subject.GpSegStart.createParser()
        options, args = parser.parse_args()

        gpsegstart = self.subject.GpSegStart.createProgram(options, args)
        with patch('gpsegstart.base.WorkerPool.return_value.getCompletedItems', return_value=[mock_get_results]):
            exitCode = gpsegstart.run()

        self.assertEqual(exitCode, 1)
        for result in gpsegstart.overall_status.results:
            if result.datadir == self.failing_data_dir:
                self.assertTrue(result.reasoncode == gp.SEGSTART_ERROR_PG_CTL_FAILED)
            else:
                self.assertTrue(result.reasoncode == gp.SEGSTART_SUCCESS)
