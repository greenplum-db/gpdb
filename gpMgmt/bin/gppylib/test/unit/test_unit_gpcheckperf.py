import imp
import os
import sys
from mock import patch, MagicMock
from gppylib.test.unit.gp_unittest import GpTestCase,run_tests
from gppylib.util import ssh_utils

class GpCheckPerf(GpTestCase):
    def setUp(self):
        gpcheckperf_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpcheckperf")
        self.subject = imp.load_source('gpcheckperf', gpcheckperf_file)
        self.mocked_hostlist = MagicMock()
        ssh_utils.HostList = MagicMock(return_value=self.mocked_hostlist)


    def tearDown(self):
        super(GpCheckPerf, self).tearDown()

    @patch('gpcheckperf.getPlatform', return_value='darwin')
    @patch('gpcheckperf.run')
    def test_get_memory_on_darwin(self, mock_run, mock_get_platform):
        mock_run.return_value = [1, 'hw.physmem: 1234']
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, None)

        mock_run.return_value = [0, 'hw.physmem: 0']
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, None)

        mock_run.return_value = [0, 'hw.physmem: 1234']
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, 1234)

    @patch('gpcheckperf.getPlatform', return_value='linux')
    @patch('gpcheckperf.run')
    def test_get_memory_on_linux(self, mock_run, mock_get_platform):
        mock_run.return_value = [1, 'MemTotal:        10 kB']
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, None)

        mock_run.return_value = [0, 'MemTotal:        0 kB']
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, None)

        mock_run.return_value = [0, 'MemTotal:        10 kB']
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, 10240)

    @patch('gpcheckperf.getPlatform', return_value='abc')
    def test_get_memory_on_invalid_platform(self, mock_get_platform):
        actual_result = self.subject.getMemory()
        self.assertEquals(actual_result, None)

    @patch('gpcheckperf.getMemory', return_value=None)
    def test_parseCommandLine_when_get_memory_fails(self, mock_get_memory):
        sys.argv = ["gpcheckperf", "-h", "locahost", "-r", "d", "-d", "/tmp"]
        with self.assertRaises(SystemExit) as e:
            self.subject.parseCommandLine()

        self.assertEqual(e.exception.code, '[Error] could not get system memory size. Instead, you can use the -S option to provide the file size value')

    @patch('gpcheckperf.getMemory', return_value=123)
    def test_parseCommandLine_when_get_memory_succeeds(self, mock_get_memory):
        sys.argv = ["gpcheckperf", "-h", "locahost", "-r", "d", "-d", "/tmp"]
        self.subject.parseCommandLine()
        self.assertEqual(self.subject.GV.opt['-S'], 246.0)

    @patch('gppylib.commands.unix.isScpEnabled', return_value=False)
    @patch('gpcheckperf.gpsync', return_value=(False, None))
    @patch('gpcheckperf.getHostList', return_value=['localhost'])
    def test_scp_not_enabled(self, mock_hostlist, mock_gpsync, mock_isScpEnabled):
        src = '%s/lib/multidd' % os.path.abspath(os.path.dirname(__file__) + "/../../../")
        target = '/tmp/gpcheckperf_$USER/multidd'
        sys.argv = ["gpcheckperf", "-h", "locahost", "-r", "d", "-d", "/tmp"]

        self.subject.main()
        mock_gpsync.assert_called_with(src, target)

    @patch('gppylib.commands.unix.isScpEnabled', return_value=True)
    @patch('gpcheckperf.gpscp', return_value=(False, None))
    @patch('gpcheckperf.getHostList', return_value=['localhost'])
    def test_scp_enabled(self, mock_hostlist, mock_gpscp, mock_isScpEnabled):
        src = '%s/lib/multidd' % os.path.abspath(os.path.dirname(__file__) + "/../../../")
        target = '=:/tmp/gpcheckperf_$USER/multidd'
        sys.argv = ["gpcheckperf", "-h", "locahost", "-r", "d", "-d", "/tmp"]

        self.subject.main()
        mock_gpscp.assert_called_with(src, target)

    @patch('gpcheckperf.getHostList', return_value=['localhost', "invalid_host"])
    def test_gpsync_failed_to_copy(self, mock_hostlist):
        src = '%s/lib/multidd' % os.path.abspath(os.path.dirname(__file__) + "/../../../")
        target = '=:tmp/'
        with self.assertRaises(SystemExit) as e:
            self.subject.gpsync(src, target)
        self.assertIn('[Error] command failed for host:invalid_host', e.exception.code)


    def test_get_host_list_with_host_file(self):
        self.subject.GV.opt = {'-f': 'hostfile.txt', '-h': ['host1', 'host2']}
        self.mocked_hostlist.filterMultiHomedHosts.return_value = ['host3', 'host4']

        result = self.subject.getHostList()

        self.assertEqual(result, ['host3', 'host4'])
        self.mocked_hostlist.parseFile.assert_called_with('hostfile.txt')
        self.mocked_hostlist.checkSSH.assert_called()


    def test_get_host_list_without_host_file(self):
        self.subject.GV.opt = {'-f': '', '-h': ['host1', 'host2']}
        self.mocked_hostlist.filterMultiHomedHosts.return_value = ['host1', 'host2']

        result = self.subject.getHostList()

        self.assertEqual(result, ['host1', 'host2'])
        self.mocked_hostlist.add.assert_any_call('host1')
        self.mocked_hostlist.add.assert_any_call('host2')
        self.mocked_hostlist.checkSSH.assert_called()


    def test_get_host_list_with_ssh_error(self):
        self.mocked_hostlist.checkSSH.side_effect = ssh_utils.SSHError("Test ssh error")

        with self.assertRaises(SystemExit) as e:
            self.subject.getHostList()

        self.assertEqual(e.exception.code, '[Error] Test ssh error')

if __name__ == '__main__':
    run_tests()
