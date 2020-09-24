from mock import patch
from test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.commands.base import CommandResult
from gppylib.commands.unix import Ping


class PingTestCase(GpTestCase):
    ping_command = None

    def set_successful_command_results(self, _):
        self.ping_command.set_results(CommandResult(0, b'', b'', True, True))

    def setUp(self):
        self.apply_patches([
            # used for failure tests, re-patched for success in success tests
            patch('gppylib.commands.unix.socket.getaddrinfo', side_effect=Exception('Test Exception')),
            # always succeed if we call the superclass run() method
            patch('gppylib.operations.unix.Command.run', side_effect=self.set_successful_command_results),
        ])

    def test_ping_cmdStr_is_correct(self):
        ping_command = Ping("testing", "doesNotExist.foo.com")
        self.assertIn("-c 1 doesNotExist.foo.com", ping_command.cmdStr)

    # patch the patch to provide success
    @patch('gppylib.commands.unix.socket.getaddrinfo', return_value=[(2, 2, 17, '', ('172.217.6.46', 0)), (2, 1, 6, '', ('172.217.6.46', 0)), (30, 2, 17, '', ('2607:f8b0:4005:809::200e', 0, 0, 0)), (30, 1, 6, '', ('2607:f8b0:4005:809::200e', 0, 0, 0))])
    def test_happy_path_without_validation_pings(self, socket):
        self.ping_command = Ping("testing", "google.com")
        self.ping_command.run(validateAfter=False)
        self.assertEqual(self.ping_command.get_results().rc, 0)

    # patch the patch to provide success
    @patch('gppylib.commands.unix.socket.getaddrinfo', return_value=[(2, 2, 17, '', ('172.217.6.46', 0)), (2, 1, 6, '', ('172.217.6.46', 0)), (30, 2, 17, '', ('2607:f8b0:4005:809::200e', 0, 0, 0)), (30, 1, 6, '', ('2607:f8b0:4005:809::200e', 0, 0, 0))])
    def test_happy_path_with_validation_pings(self, socket):
        self.ping_command = Ping("testing", "google.com")
        self.ping_command.run(validateAfter=True)
        self.assertEqual(self.ping_command.get_results().rc, 0)

    def test_ping_survives_dns_failure(self):
        ping_command = Ping("testing", "doesNotExist.foo.com")
        ping_command.run(validateAfter=False)
        self.assertEqual(ping_command.get_results().rc, 1)
        self.assertIn("Failed to get ip address", ping_command.get_results().stderr)

    def test_ping_when_validating_fails_on_dns_failure(self):
        ping_command = Ping("testing", "doesNotExist.foo.com")
        with self.assertRaisesRegex(Exception, 'Test Exception'):
            ping_command.run(validateAfter=True)


if __name__ == '__main__':
    run_tests()
