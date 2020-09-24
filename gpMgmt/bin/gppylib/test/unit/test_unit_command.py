from commands.base import Command, CommandResult, REMOTE, GPHOME
from .gp_unittest import *


class CommandTest(GpTestCase):
    def setUp(self):
        self.subject = Command("my name", "my command string")

    def test_get_stdout_before_running_raises(self):
        with self.assertRaisesRegex(Exception, "command not run yet"):
            self.subject.get_stdout()

    def test_get_stdout_after_running_returns_stdout(self):
        self.subject.set_results(CommandResult(0, b"my stdout", b"", True, False))

        self.assertEqual(self.subject.get_stdout(), "my stdout")

    def test_get_stdout_after_running_returns_stripped_stdout(self):
        self.subject.set_results(CommandResult(0, b"  my stdout\n", b"", True, False))

        self.assertEqual(self.subject.get_stdout(), "my stdout")

    def test_get_stdout_after_running_with_no_strip_returns_unstripped_stdout(self):
        self.subject.set_results(CommandResult(0, b"  my stdout\n", b"", True, False))

        self.assertEqual(self.subject.get_stdout(strip=False), "  my stdout\n")

    def test_get_return_code_before_running_raises(self):
        with self.assertRaisesRegex(Exception, "command not run yet"):
            self.subject.get_return_code()

    def test_get_stdout_after_running_returns_rc(self):
        self.subject.set_results(CommandResult(-23, b"my stdout", b"", True, False))

        self.assertEqual(self.subject.get_return_code(), -23)

    def test_get_stderr_before_running_raises(self):
        with self.assertRaisesRegex(Exception, "command not run yet"):
            self.subject.get_stderr()

    def test_get_stderr_after_running_returns_stderr(self):
        self.subject.set_results(CommandResult(0, b"", b"my stderr", True, False))

        self.assertEqual(self.subject.get_stderr(), "my stderr")

    def test_create_command_with_default_gphome(self):
        self.subject = Command("my name", "my command string", ctxt=REMOTE, remoteHost="someHost")
        self.assertEqual(GPHOME, self.subject.exec_context.gphome)

    def test_create_command_with_custom_gphome(self):
        self.subject = Command("my name", "my command string", ctxt=REMOTE, remoteHost="someHost", gphome="/new/gphome")
        self.assertIn("/new/gphome", self.subject.exec_context.gphome)

    def test_running_command_remembers_pid(self):
        self.subject = Command("my name", "ls")
        self.subject.pid = -1
        self.subject.run()
        self.assertTrue(self.subject.pid > 0)

if __name__ == '__main__':
    run_tests()
