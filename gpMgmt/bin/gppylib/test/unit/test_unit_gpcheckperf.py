import imp
import os
import sys
from mock import patch
from gppylib.test.unit.gp_unittest import GpTestCase,run_tests

class GpCheckPerf(GpTestCase):
    def setUp(self):
        gpcheckcat_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpcheckperf")
        self.subject = imp.load_source('gpcheckperf', gpcheckcat_file)

    def tearDown(self):
        super(GpCheckPerf, self).tearDown()

    @patch('gpcheckperf.getPlatform', return_value='darwin')
    @patch('gpcheckperf.run')
    def test_get_memory_on_darwin(self, mock_run, mock_get_platform):
        mock_run.return_value = [1, 'hw.physmem: 1234']
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, None)

        mock_run.return_value = [0, 'hw.physmem: 0']
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, None)

        mock_run.return_value = [0, 'hw.physmem: 1234']
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, 1234)

    @patch('gpcheckperf.getPlatform', return_value='linux')
    @patch('gpcheckperf.run')
    def test_get_memory_on_linux(self, mock_run, mock_get_platform):
        mock_run.return_value = [1, 'MemTotal:        10 kB']
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, None)

        mock_run.return_value = [0, 'MemTotal:        0 kB']
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, None)

        mock_run.return_value = [0, 'MemTotal:        10 kB']
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, 10240)

    @patch('gpcheckperf.getPlatform', return_value='abc')
    def test_get_memory_on_invalid_platform(self, mock_get_platform):
        actual_result = self.subject.getMemory()
        self.assertEqual(actual_result, None)

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

    def test_parseMultiDDResult_when_output_regular(self):
        inputText = """[localhost] dd if=/dev/zero of=/tmp/gpcheckperf_gpadmin/ddfile count=131072 bs=32768
[localhost] 131072+0 records in
[localhost] 131072+0 records out
[localhost] 4294967296 bytes transferred in 2.973025 secs (1444645536 bytes/sec)
[localhost]
[localhost] multidd total bytes  4294967296
[localhost] real 3.65
[localhost] user 0.18
[localhost] sys 2.52
    """
        actual_result = self.subject.parseMultiDDResult(inputText)
        (mbps, time, bytes) = actual_result["localhost"]
        exp_mbps, exp_time, exp_bytes = (1122.1917808219177, 3.65, 4294967296)
        self.assertEqual(mbps, exp_mbps)
        self.assertEqual(time, exp_time)
        self.assertEqual(bytes, exp_bytes)

    def test_parseMultiDDResult_when_output_comma(self):
        inputText = """[localhost] dd if=/dev/zero of=/tmp/gpcheckperf_gpadmin/ddfile count=131072 bs=32768
[localhost] 131072+0 records in
[localhost] 131072+0 records out
[localhost] 4294967296 bytes transferred in 2.973025 secs (1444645536 bytes/sec)
[localhost]
[localhost] multidd total bytes  4294967296
[localhost] real 3,65
[localhost] user 0,18
[localhost] sys 2,52
    """
        actual_result = self.subject.parseMultiDDResult(inputText)
        (mbps, time, bytes) = actual_result["localhost"]
        exp_mbps, exp_time, exp_bytes = (1122.1917808219177, 3.65, 4294967296)
        self.assertEqual(mbps, exp_mbps)
        self.assertEqual(time, exp_time)
        self.assertEqual(bytes, exp_bytes)

if __name__ == '__main__':
    run_tests()
