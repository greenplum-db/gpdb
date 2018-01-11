#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2014. All Rights Reserved. 
#

from gppylib.commands.base import CommandResult
from gppylib.operations.unix import CleanSharedMem
from mock import Mock, MagicMock, patch

from test.unit.gp_unittest import GpTestCase, run_tests


class CleanSharedMemTestCase(GpTestCase):
    def _get_mock_segment(self, name, datadir, port, hostname, address):
        m = Mock()
        m.name = name
        m.datadir = datadir
        m.port = port
        m.hostname = hostname
        m.address = address
        return m

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.unix.WorkerPool')
    def test_run(self, mock1, mock2):
        segments = [self._get_mock_segment('seg1', '/tmp/gpseg1', 1234, 'host1', 'host1')]
        c = CleanSharedMem(segments)
        file_contents = 'asdfads\nasdfsd asdfadsf\n12345 23456'.split()
        m = MagicMock()
        m.return_value.__enter__.return_value.readlines.return_value = file_contents
        with patch('__builtin__.open', m, create=True):
            c.run()

    @patch('os.path.isfile', return_value=False)
    @patch('gppylib.operations.unix.WorkerPool')
    def test_run_with_no_pid_file(self, mock1, mock2):
        segments = [self._get_mock_segment('seg1', '/tmp/gpseg1', 1234, 'host1', 'host1')]
        c = CleanSharedMem(segments)
        c.run()

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.unix.Command.get_results', return_value=CommandResult(1, '', '', False, False))
    def test_run_with_invalid_pid_file(self, mock1, mock2):
        segments = [self._get_mock_segment('seg1', '/tmp/gpseg1', 1234, 'host1', 'host1')]
        c = CleanSharedMem(segments)
        file_contents = 'asdfadsasdfasdf'.split()
        m = MagicMock()
        m.return_value.__enter__.return_value.readlines.return_value = file_contents
        with patch('__builtin__.open', m, create=True):
            with self.assertRaisesRegexp(Exception, 'Unable to clean up shared memory for segment'):
                c.run()

    @patch('os.path.isfile', return_value=True)
    @patch('gppylib.operations.unix.Command.run')
    @patch('gppylib.operations.unix.Command.get_results', return_value=CommandResult(1, '', '', False, False))
    def test_run_with_error_in_workerpool(self, mock1, mock2, mock3):
        segments = [self._get_mock_segment('seg1', '/tmp/gpseg1', 1234, 'host1', 'host1')]
        c = CleanSharedMem(segments)
        file_contents = 'asdfads\nasdfsd asdfadsf\n12345 23456'.split()
        m = MagicMock()
        m.return_value.__enter__.return_value.readlines.return_value = file_contents
        with patch('__builtin__.open', m, create=True):
            with self.assertRaisesRegexp(Exception, 'Unable to clean up shared memory'):
                c.run()


if __name__ == '__main__':
    run_tests()
