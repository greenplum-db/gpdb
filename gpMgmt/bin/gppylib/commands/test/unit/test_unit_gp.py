#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved. 
#

from gppylib.commands.base import CommandResult
from mock import patch, mock_open

from gppylib.commands.gp import is_pid_postmaster, get_postmaster_pid_locally, get_postgres_segment_processes, is_gprecoverseg_running
from test.unit.gp_unittest import GpTestCase, run_tests


class GpCommandTestCase(GpTestCase):

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"1234", b"", True, False)])
    def test_is_pid_postmaster_pid_exists(self, mock1, mock2):
        pid = 1234
        datadir = '/data/primary/gpseg0'
        self.assertTrue(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_pid_doesnt_exists(self, mock1, mock2):
        pid = 1234
        datadir = '/data/primary/gpseg0'
        self.assertFalse(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"1234", b"", True, False)])
    def test_is_pid_postmaster_pid_as_string(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0'
        self.assertTrue(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_no_result(self, mock1, mock2):
        pid = '1234'
        datadir = None
        self.assertFalse(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(127, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_no_pgrep(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0' 
        self.assertTrue(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(127, b"", b"", False, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_no_pwdx(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0' 
        self.assertTrue(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run', side_effect=[None, None, Exception('Error')]) 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(1, b"", b"", False, False)])
    def test_is_pid_postmaster_pgrep_failed(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0' 
        self.assertTrue(is_pid_postmaster(datadir, pid))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"1234", b"", True, False)])
    def test_is_pid_postmaster_pid_exists_remote(self, mock1, mock2):
        pid = 1234
        datadir = '/data/primary/gpseg0'
        remoteHost = 'scdw'
        self.assertTrue(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_pid_doesnt_exists_remote(self, mock1, mock2):
        pid = 1234
        datadir = '/data/primary/gpseg0'
        remoteHost = 'scdw'
        self.assertFalse(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"1234", b"", True, False)])
    def test_is_pid_postmaster_pid_as_string_remote(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0'
        remoteHost = 'scdw'
        self.assertTrue(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_no_result_remote(self, mock1, mock2):
        pid = '1234'
        datadir = None
        remoteHost = 'scdw'
        self.assertFalse(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(127, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_no_pgrep_remote(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0' 
        remoteHost = 'scdw'
        self.assertTrue(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run') 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(127, b"", b"", False, False),
                                                                   CommandResult(0, b"", b"", True, False)])
    def test_is_pid_postmaster_no_pwdx_remote(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0' 
        remoteHost = 'scdw'
        self.assertTrue(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run', side_effect=[None, None, Exception('Error')]) 
    @patch('gppylib.commands.gp.Command.get_results', side_effect=[CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(0, b"", b"", True, False),
                                                                   CommandResult(1, b"", b"", False, False)])
    def test_is_pid_postmaster_pgrep_failed_remote(self, mock1, mock2):
        pid = '1234'
        datadir = '/data/primary/gpseg0' 
        remoteHost = 'scdw'
        self.assertTrue(is_pid_postmaster(datadir, pid, remoteHost=remoteHost))

    @patch('gppylib.commands.gp.Command.run')
    @patch('gppylib.commands.gp.Command.get_results', return_value=CommandResult(0, b"gpadmin  1234     1  0 18:48 ?        00:00:00 /usr/local/greenplum-db-devel/bin/postgres -D /tmp -p", b"", True, False))
    def test_get_postmaster_pid_locally(self, mock1, mock2):
        self.assertEqual(get_postmaster_pid_locally('/tmp'), 1234)

    @patch('gppylib.commands.gp.Command.run', side_effect=Exception('Error'))
    def test_get_postmaster_pid_locally_error(self, mock1):
        self.assertEqual(get_postmaster_pid_locally('/tmp'), -1)

    @patch('gppylib.commands.gp.Command.run', return_value=CommandResult(0, b"", b"", True, False))
    def test_get_postmaster_pid_locally_empty(self, mock1):
        self.assertEqual(get_postmaster_pid_locally('/tmp'), -1)

    @patch('gppylib.commands.gp.getPostmasterPID', return_value=-1)
    def test_get_postgres_segment_processes_no_postmaster(self, mock1):
        result = get_postgres_segment_processes('/data/primary/gpseg0', 'sdw1')
        self.assertEqual(result, [])

    @patch('gppylib.commands.gp.getPostmasterPID', return_value=1234)
    @patch('gppylib.commands.gp.Command.run')
    @patch('gppylib.commands.gp.Command.get_results', return_value=CommandResult(0, b"\n111\n222\n333", b"", True, False))
    def test_get_postgres_segment_processes_succeeds(self, mock1, mock2, mock3):
        result = get_postgres_segment_processes('/data/primary/gpseg0', 'sdw1')
        self.assertEqual(result, [1234, 111, 222, 333])

    @patch('gppylib.commands.gp.getPostmasterPID', return_value=1234)
    @patch('gppylib.commands.gp.Command.run')
    @patch('gppylib.commands.gp.Command.get_results', return_value=CommandResult(0, b"\n111\nabc\n222\n", b"", True, False))
    def test_get_postgres_segment_processes_with_str_pgrep_output(self, mock1, mock2, mock3):
        result = get_postgres_segment_processes('/data/primary/gpseg0', 'sdw1')
        self.assertEqual(result, [1234, 111, 222])

    @patch('gppylib.commands.gp.getPostmasterPID', return_value=1234)
    @patch('gppylib.commands.gp.Command.run')
    @patch('gppylib.commands.gp.Command.get_results', return_value=CommandResult(1, b"", b"", False, False))
    def test_get_postgres_segment_processes_when_pgrep_fails(self, mock1, mock2, mock3):
        result = get_postgres_segment_processes('/data/primary/gpseg0', 'sdw1')
        self.assertEqual(result, [1234])

    @patch('gppylib.commands.gp.check_pid', return_value=True)
    @patch('gppylib.commands.gp.get_coordinatordatadir')
    @patch("builtins.open", new_callable=mock_open, read_data="123")
    def test_is_gprecoverseg_running_succeeds(self, mock_file, mock1, mock2):
        result = is_gprecoverseg_running()
        mock2.assert_called_once_with('123')
        self.assertTrue(result)

    @patch('gppylib.commands.gp.check_pid')
    @patch('gppylib.commands.gp.get_coordinatordatadir', return_value='/invalid/path/')
    def test_is_gprecoverseg_running_when_pidfile_does_not_exists(self, mock1, mock2):
        result = is_gprecoverseg_running()
        self.assertFalse(result)
        self.assertFalse(mock2.called)


if __name__ == '__main__':
    run_tests()
