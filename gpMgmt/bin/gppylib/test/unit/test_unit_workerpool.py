import os
import io
import threading
import time
import unittest

import mock

from gppylib.commands.base import Command, ExecutionError, WorkerPool, \
                                  join_and_indicate_progress

class WorkerPoolTest(unittest.TestCase):
    def setUp(self):
        self.pool = WorkerPool(numWorkers=1, logger=mock.Mock())

    def tearDown(self):
        # All background threads must be stopped, or else the test runner will
        # hang waiting. Join the stopped threads to make sure we're completely
        # clean for the next test.
        self.pool.haltWork()
        self.pool.joinWorkers()

    def test_pool_must_have_some_workers(self):
        with self.assertRaises(Exception):
            WorkerPool(numWorkers=0)
        
    def test_pool_runs_added_command(self):
        cmd = mock.Mock(spec=Command)

        self.pool.addCommand(cmd)
        self.pool.join()

        cmd.run.assert_called_once_with()

    def test_completed_commands_are_retrievable(self):
        cmd = mock.Mock(spec=Command)

        self.pool.addCommand(cmd) # should quickly be completed
        self.pool.join()

        self.assertEqual(self.pool.getCompletedItems(), [cmd])

    def test_pool_is_not_marked_done_until_commands_finish(self):
        cmd = mock.Mock(spec=Command)

        # cmd.run() will block until this Event is set.
        event = threading.Event()
        def wait_for_event():
            event.wait()
        cmd.run.side_effect = wait_for_event

        self.assertTrue(self.pool.isDone())

        try:
            self.pool.addCommand(cmd)
            self.assertFalse(self.pool.isDone())

        finally:
            # Make sure that we unblock the thread even on a test failure.
            event.set()

        self.pool.join()

        self.assertTrue(self.pool.isDone())

    def test_pool_can_be_emptied_of_completed_commands(self):
        cmd = mock.Mock(spec=Command)

        self.pool.addCommand(cmd)
        self.pool.join()

        self.pool.empty_completed_items()
        self.assertEqual(self.pool.getCompletedItems(), [])

    def test_check_results_succeeds_when_no_items_fail(self):
        cmd = mock.Mock(spec=Command)

        # Command.get_results() returns a CommandResult.
        # CommandResult.wasSuccessful() should return True if the command
        # succeeds.
        result = cmd.get_results.return_value
        result.wasSuccessful.return_value = True

        self.pool.addCommand(cmd)
        self.pool.join()
        self.pool.check_results()

    def test_check_results_throws_exception_at_first_failure(self):
        cmd = mock.Mock(spec=Command)

        # Command.get_results() returns a CommandResult.
        # CommandResult.wasSuccessful() should return False to simulate a
        # failure.
        result = cmd.get_results.return_value
        result.wasSuccessful.return_value = False

        self.pool.addCommand(cmd)
        self.pool.join()

        with self.assertRaises(ExecutionError):
            self.pool.check_results()

    def test_join_with_timeout_returns_done_immediately_if_there_is_nothing_to_do(self):
        start = time.time()
        done = self.pool.join(10)
        delta = time.time() - start

        self.assertTrue(done)

        # "Returns immediately" is a difficult thing to test. Longer than two
        # seconds seems like a reasonable failure case, even on a heavily loaded
        # test container.
        self.assertLess(delta, 2)

    def test_join_with_timeout_doesnt_return_done_until_all_commands_complete(self):
        cmd = mock.Mock(spec=Command)

        # cmd.run() will block until this Event is set.
        event = threading.Event()
        def wait_for_event():
            event.wait()
        cmd.run.side_effect = wait_for_event

        try:
            self.pool.addCommand(cmd)

            done = self.pool.join(0.001)
            self.assertFalse(done)

            # Test zero and negative timeouts too.
            done = self.pool.join(0)
            self.assertFalse(done)

            done = self.pool.join(-1)
            self.assertFalse(done)

        finally:
            # Make sure that we unblock the thread even on a test failure.
            event.set()

        done = self.pool.join(2) # should be immediate, but there's still a race
        self.assertTrue(done)

    def test_completed_returns_number_of_completed_commands(self):
        for _ in range(3):
            cmd = mock.Mock(spec=Command)
            self.pool.addCommand(cmd)

        self.pool.join()
        self.assertEqual(self.pool.completed, 3)

    def test_completed_can_be_cleared_back_to_zero(self):
        for _ in range(3):
            cmd = mock.Mock(spec=Command)
            self.pool.addCommand(cmd)

        self.pool.join()
        self.pool.empty_completed_items()
        self.assertEqual(self.pool.completed, 0)

    def test_completed_is_reset_to_zero_after_getCompletedItems(self):
        for _ in range(3):
            cmd = mock.Mock(spec=Command)
            self.pool.addCommand(cmd)

        self.pool.join()
        self.pool.getCompletedItems()
        self.assertEqual(self.pool.completed, 0)

    def test_assigned_returns_number_of_assigned_commands(self):
        for _ in range(3):
            cmd = mock.Mock(spec=Command)
            self.pool.addCommand(cmd)

        self.assertEqual(self.pool.assigned, 3)

    def test_assigned_is_decremented_when_completed_items_are_emptied(self):
        for _ in range(3):
            cmd = mock.Mock(spec=Command)
            self.pool.addCommand(cmd)

        self.pool.join()
        self.pool.empty_completed_items()

        self.assertEqual(self.pool.assigned, 0)

    def test_assigned_is_decremented_when_completed_items_are_checked(self):
        cmd = mock.Mock(spec=Command)

        # Command.get_results() returns a CommandResult.
        # CommandResult.wasSuccessful() should return True if the command
        # succeeds.
        result = cmd.get_results.return_value
        result.wasSuccessful.return_value = True

        self.pool.addCommand(cmd)

        self.pool.join()
        self.pool.check_results()

        self.assertEqual(self.pool.assigned, 0)

    def test_assigned_is_decremented_when_completed_items_are_popped(self):
        # The first command will finish immediately.
        cmd1 = mock.Mock(spec=Command)
        self.pool.addCommand(cmd1)

        # The other command will wait until we allow it to continue.
        cmd2 = mock.Mock(spec=Command)

        # cmd.run() will block until this Event is set.
        event = threading.Event()
        def wait_for_event():
            event.wait()
        cmd2.run.side_effect = wait_for_event

        try:
            self.pool.addCommand(cmd2)
            self.assertEqual(self.pool.assigned, 2)

            # Avoid race flakes; make sure we actually complete the first
            # command.
            while self.pool.completed < 1:
                self.pool.join(0.001)

            # Pop the completed item.
            self.assertEqual(self.pool.getCompletedItems(), [cmd1])

            # Now we should be down to one assigned command.
            self.assertEqual(self.pool.assigned, 1)

        finally:
            # Make sure that we unblock the thread even on a test failure.
            event.set()

        self.pool.join()

        # Pop the other completed item.
        self.assertEqual(self.pool.getCompletedItems(), [cmd2])
        self.assertEqual(self.pool.assigned, 0)

    def test_join_and_indicate_progress_prints_nothing_if_pool_is_done(self):
        stdout = io.StringIO()
        join_and_indicate_progress(self.pool, stdout)

        self.assertEqual(stdout.getvalue(), '')

    def test_join_and_indicate_progress_prints_dots_until_pool_is_done(self):
        cmd = mock.Mock(spec=Command)

        # cmd.run() will block until this Event is set.
        event = threading.Event()
        def wait_for_event():
            event.wait()
        cmd.run.side_effect = wait_for_event

        # Open up a pipe and wrap each end in a file-like object.
        read_end, write_end = os.pipe()
        read_end = os.fdopen(read_end, 'r')
        write_end = os.fdopen(write_end, 'w')

        # Create a thread to perform join_and_indicate_progress().
        def tmain():
            join_and_indicate_progress(self.pool, write_end, interval=0.001)
            write_end.close()
        join_thread = threading.Thread(target=tmain)

        try:
            # Add the command, then join the WorkerPool.
            self.pool.addCommand(cmd)
            join_thread.start()

            # join_and_indicate_progress() is now writing to our pipe. Wait for
            # a few dots...
            for _ in range(3):
                byte = read_end.read(1)
                self.assertEqual(byte, '.')

            # ...then stop the command.
            event.set()

            # Make sure the rest of the output consists of dots ending in a
            # newline. (tmain() closes the write end of the pipe so that this
            # read() will complete.)
            remaining = read_end.read()
            self.assertRegex(remaining, r'^[.]*\n$')

        finally:
            # Make sure that we unblock and join all threads, even on a test
            # failure.
            event.set()
            join_thread.join()

    def test_join_and_indicate_progress_flushes_every_dot(self):
        duration = 0.005

        cmd = mock.Mock(spec=Command)
        def wait_for_duration():
            time.sleep(duration)
        cmd.run.side_effect = wait_for_duration
        self.pool.addCommand(cmd)

        stdout = mock.Mock(io.StringIO())
        join_and_indicate_progress(self.pool, stdout, interval=(duration / 5))

        for i, call in enumerate(stdout.mock_calls):
            # Every written dot should be followed by a flush().
            if call == mock.call.write('.'):
                self.assertEqual(stdout.mock_calls[i + 1], mock.call.flush())
