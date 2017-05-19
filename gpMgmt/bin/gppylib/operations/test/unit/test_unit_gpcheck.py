from gppylib.commands.base import Command, REMOTE
from gppylib.operations.gpcheck import get_host_for_command, get_command, get_copy_command

from test.unit.gp_unittest import GpTestCase, run_tests


class GpCheckTestCase(GpTestCase):
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

if __name__ == '__main__':
    run_tests()
