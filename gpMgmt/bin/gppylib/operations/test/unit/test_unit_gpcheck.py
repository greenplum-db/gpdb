import imp
import logging
import mock
import os

from gppylib.commands.base import Command, REMOTE
from gppylib.operations.gpcheck import get_host_for_command, get_command, get_copy_command
from test.unit.gp_unittest import GpTestCase, run_tests

gpcheck_file = os.path.abspath(os.path.dirname(__file__) + "/../../../../gpcheck")

class GpCheckTestCase(GpTestCase):
    def setUp(self):
        # Because gpcheck does not have a .py extension, we have to use imp to
        # import it. If we had a gpcheck.py, this is equivalent to:
        #   import gpcheck
        #   self.subject = gpcheck
        self.gpcheck = imp.load_source('gpcheck', gpcheck_file)
        self.gpcheck.logger = mock.MagicMock(logging.Logger)
        self.gpcheck.gpcheck_config = self.gpcheck.GpCheckConfig()

    def test_get_host_for_command_uses_supplied_remote_host(self):
        cmd = Command('name', 'hostname', ctxt=REMOTE, remoteHost='foo') 
        result = get_host_for_command(False, cmd)
        expected_result = 'foo'
        self.assertEqual(result, expected_result)

    def test_get_host_for_command_for_local_uses_local_hostname(self):
        cmd = Command('name', 'hostname') 
        cmd.run(validateAfter=True)
        hostname = cmd.get_results().stdout.strip()
        result = get_host_for_command(True, cmd)
        expected_result = hostname 
        self.assertEqual(result, expected_result)

    def test_get_command_creates_command_with_parameters_supplied(self):
        host = 'foo'
        cmd = 'bar'
        result = get_command(True, cmd, host)
        expected_result = Command(host, cmd)
        self.assertEqual(result.name, expected_result.name)
        self.assertEqual(result.cmdStr, expected_result.cmdStr)

    def test_get_command_creates_command_with_remote_params_supplied(self):
        host = 'foo'
        cmd = 'bar'
        result = get_command(False, cmd, host)
        expected_result = Command(host, cmd, ctxt=REMOTE, remoteHost=host)
        self.assertEqual(result.name, expected_result.name)
        self.assertEqual(result.cmdStr, expected_result.cmdStr)

    def test_get_copy_command_when_remote_does_scp(self):
        host = 'foo'
        datafile = 'bar'
        tmpdir = '/tmp/foobar'
        result = get_copy_command(False, host, datafile, tmpdir)
        expected_result = Command(host, 'scp %s:%s %s/%s.data' % (host, datafile, tmpdir, host))
        self.assertEqual(result.name, expected_result.name)
        self.assertEqual(result.cmdStr, expected_result.cmdStr)

    def test_get_copy_command_when_local_does_mv(self):
        host = 'foo'
        datafile = 'bar'
        tmpdir = '/tmp/foobar'
        result = get_copy_command(True, host, datafile, tmpdir)
        expected_result = Command(host, 'mv -f %s %s/%s.data' % (datafile, tmpdir, host))
        self.assertEqual(result.name, expected_result.name)
        self.assertEqual(result.cmdStr, expected_result.cmdStr)

    def test_sysctl_succeeds_when_config_values_match(self):
        self.gpcheck.printError = mock.MagicMock()

        localhost = mock.MagicMock()
        localhost.hostname = "localhost"
        localhost.data.sysctl.variables = {'sysctl.net.ipv4.tcp_syncookies': '1'}

        self.gpcheck.gpcheck_config.expectedSysctlValues = {'sysctl.net.ipv4.tcp_syncookies': '1'}

        self.gpcheck.testSysctl(localhost)
        self.gpcheck.printError.assert_not_called()
        self.assertFalse(self.gpcheck.found_errors)

    def test_sysctl_prints_error_when_config_values_dont_match(self):
        localhost = mock.MagicMock()
        localhost.hostname = "localhost"
        localhost.data.sysctl.variables = {'sysctl.net.ipv4.ip_local_port_range': '10000 65535'}

        self.gpcheck.gpcheck_config.expectedSysctlValues = {'sysctl.net.ipv4.ip_local_port_range': '1 60000'}

        self.gpcheck.testSysctl(localhost)
        self.assertTrue(self.gpcheck.found_errors)
        self.gpcheck.logger.error.assert_called_once_with("GPCHECK_ERROR host(%s): %s",
                                                          "localhost",
                                                          "/etc/sysctl.conf value for key 'sysctl.net.ipv4.ip_local_port_range' has value '10000 65535' and expects '1 60000'")

    def test_sysctl_prints_error_when_config_is_missing(self):
        localhost = mock.MagicMock()
        localhost.hostname = "localhost"
        localhost.data.sysctl.variables = {'sysctl.non.existent.setting': '10000 65535'}

        self.gpcheck.gpcheck_config.expectedSysctlValues = {'sysctl.net.ipv4.ip_local_port_range': '1 60000'}

        self.gpcheck.testSysctl(localhost)
        self.assertTrue(self.gpcheck.found_errors)
        self.gpcheck.logger.error.assert_called_once_with("GPCHECK_ERROR host(%s): %s",
                                                          "localhost",
                                                          "variable not detected in /etc/sysctl.conf: 'sysctl.net.ipv4.ip_local_port_range'")


if __name__ == '__main__':
    run_tests()
