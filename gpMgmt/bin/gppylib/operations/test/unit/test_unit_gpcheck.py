import imp
import logging
import mock
import os

from gppylib.commands.base import Command, REMOTE
from gppylib.operations.gpcheck import get_host_for_command, get_command, get_copy_command
from gppylib.gpcheckutil import GpMount
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

        self.mock_host = self.gpcheck.GpCheckHost('hostname')
        self.mock_host.data = mock.MagicMock()
        self.mock_host.data.mounts = mock.MagicMock()
        data_mount = GpMount()
        data_mount.partition = "/data"
        data_mount.type = "xfs"
        data_mount.options = set(["defaults"])
        tmp_mount = GpMount()
        tmp_mount.partition = "/tmp"
        tmp_mount.type = "xfs"
        tmp_mount.options = set(["rw", "noatime"])
        self.mock_host.data.mounts.entries = {'/data': data_mount, "/tmp": tmp_mount}
        self.gpcheck.gpcheck_config = self.gpcheck.GpCheckConfig()
        self.gpcheck.gpcheck_config.xfs_mount_options = {}

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
        localhost.hostname = "hostname"
        localhost.data.sysctl.variables = {'sysctl.net.ipv4.ip_local_port_range': '10000 65535'}

        self.gpcheck.gpcheck_config.expectedSysctlValues = {'sysctl.net.ipv4.ip_local_port_range': '1 60000'}

        self.gpcheck.testSysctl(localhost)
        self.assertTrue(self.gpcheck.found_errors)
        self.assertEquals(self.gpcheck.logger.error.call_count, 1)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("/etc/sysctl.conf value for key 'sysctl.net.ipv4.ip_local_port_range' has value '10000 65535' and expects '1 60000'")
        ])

    def test_sysctl_prints_error_when_config_is_missing(self):
        localhost = mock.MagicMock()
        localhost.hostname = "hostname"
        localhost.data.sysctl.variables = {'sysctl.non.existent.setting': '10000 65535'}

        self.gpcheck.gpcheck_config.expectedSysctlValues = {'sysctl.net.ipv4.ip_local_port_range': '1 60000'}

        self.gpcheck.testSysctl(localhost)
        self.assertTrue(self.gpcheck.found_errors)
        self.assertEquals(self.gpcheck.logger.error.call_count, 1)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("variable not detected in /etc/sysctl.conf: 'sysctl.net.ipv4.ip_local_port_range'")
        ])

    def test_default_xfs_mount_options_nonexistent_mount_is_ignored(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data./two/dirs'] = "re"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "defaults"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_default_xfs_mount_options_matching_values(self):
        self.mock_host.data.mounts.entries['/tmp'].options = set(["defaults"])
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "defaults"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_default_xfs_mount_options_mismatched_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "rw"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.assertEquals(self.gpcheck.logger.error.call_count, 2)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'rw'"),
            gpcheck_call("XFS filesystem on device /tmp has 2 XFS mount options and 1 are expected"),
        ])

    def test_one_mount_point_xfs_mount_options_matching_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "defaults"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_one_mount_point_xfs_mount_options_mismatched_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "rw,noatime"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.assertEquals(self.gpcheck.logger.error.call_count, 3)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("XFS filesystem on device /data has 1 XFS mount options and 2 are expected"),
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'rw'"),
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'noatime'"),
        ])

    def test_multiple_mount_points_xfs_mount_options_matching_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "defaults"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp'] = "rw,noatime"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_multiple_mount_points_xfs_mount_options_mismatched_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "rw"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp'] = "rw"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.assertEquals(self.gpcheck.logger.error.call_count, 2)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'rw'"),
            gpcheck_call("XFS filesystem on device /tmp has 2 XFS mount options and 1 are expected"),
        ])

    def test_one_mount_point_xfs_mount_options_with_defaults_matching_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "defaults"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "rw,noatime"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_one_mount_point_xfs_mount_options_with_defaults_mismatched_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "rw"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "rw"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.assertEquals(self.gpcheck.logger.error.call_count, 2)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'rw'"),
            gpcheck_call("XFS filesystem on device /tmp has 2 XFS mount options and 1 are expected"),
        ])

    def test_multiple_mount_points_xfs_mount_options_with_defaults_matching_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "defaults"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp'] = "rw,noatime"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "rw"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_multiple_mount_points_xfs_mount_options_with_defaults_mismatched_values(self):
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "rw"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp'] = "rw"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options'] = "defaults"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.assertEquals(self.gpcheck.logger.error.call_count, 2)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'rw'"),
            gpcheck_call("XFS filesystem on device /tmp has 2 XFS mount options and 1 are expected"),
        ])

    def test_nested_paths_xfs_mount_options_matching_values(self):
        above_mount = GpMount()
        above_mount.partition = "/tmp/gpdata"
        above_mount.type = "xfs"
        above_mount.options = set(["noatime"])
        self.mock_host.data.mounts.entries['/tmp/gpdata'] = above_mount
        below_mount = GpMount()
        below_mount.partition = "/gpdata/data"
        below_mount.type = "xfs"
        below_mount.options = set(["rw"])
        self.mock_host.data.mounts.entries['/gpdata/data'] = below_mount
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./gpdata/data'] = "rw"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "defaults"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp/gpdata'] = "noatime"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp'] = "rw,noatime"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.gpcheck.logger.error.assert_not_called()

    def test_nested_paths_xfs_mount_options_mismatched_values(self):
        above_mount = GpMount()
        above_mount.partition = "/tmp/gpdata"
        above_mount.type = "xfs"
        above_mount.options = set(["noatime"])
        self.mock_host.data.mounts.entries['/tmp/gpdata'] = above_mount
        below_mount = GpMount()
        below_mount.partition = "/gpdata/data"
        below_mount.type = "xfs"
        below_mount.options = set(["rw"])
        self.mock_host.data.mounts.entries['/gpdata/data'] = below_mount
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./gpdata/data'] = "noatime"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./data'] = "rw"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp/gpdata'] = "defaults"
        self.gpcheck.gpcheck_config.xfs_mount_options['xfs_mount_options./tmp'] = "rw"
        self.gpcheck.testLinuxMounts(self.mock_host)
        self.assertEquals(self.gpcheck.logger.error.call_count, 4)
        self.gpcheck.logger.error.assert_has_calls([
            gpcheck_call("XFS filesystem on device /gpdata/data is missing the recommended mount option 'noatime'"),
            gpcheck_call("XFS filesystem on device /data is missing the recommended mount option 'rw'"),
            gpcheck_call("XFS filesystem on device /tmp has 2 XFS mount options and 1 are expected"),
            gpcheck_call("XFS filesystem on device /tmp/gpdata is missing the recommended mount option 'defaults'"),
        ])

def gpcheck_call(msg):
    return mock.call("GPCHECK_ERROR host(%s): %s", "hostname", msg)

if __name__ == '__main__':
    run_tests()
