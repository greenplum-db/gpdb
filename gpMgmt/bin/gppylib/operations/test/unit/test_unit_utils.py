#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
import sys

import mock

from gppylib.commands.base import ExecutionError
from gppylib.operations.utils import RemoteOperation, ParallelOperation
from gppylib.operations.test_utils_helper import TestOperation, RaiseOperation, RaiseOperation_Unpicklable, RaiseOperation_Safe, ExceptionWithArgs
from operations.unix import ListFiles
from test.unit.gp_unittest import GpTestCase, run_tests
from psycopg2 import DatabaseError

class UtilsTestCase(GpTestCase):
    """
    Requires GPHOME set. Does actual ssh to localhost.
    """

    def setUp(self):
        self.old_sys_argv = sys.argv
        sys.argv = ['utils.py']

    def tearDown(self):
        sys.argv = self.old_sys_argv

    def test_Remote_basic(self):
        """ Basic RemoteOperation test """
        self.assertTrue(TestOperation().run() == RemoteOperation(TestOperation(), "localhost").run())

    def test_Remote_exceptions(self):
        """ Test that an Exception returned remotely will be raised locally. """
        with self.assertRaises(Exception):
            RemoteOperation(RaiseOperation(), "localhost").run()

    def test_proper_exceptions_sanity(self):
        try:
            RemoteOperation(RaiseOperation_Safe(), "localhost").run()
        except ExceptionWithArgs as e:
            pass
        else:
            self.fail("ExceptionWithArgs should have been successfully raised + caught, because proper idiom is used.")

    def test_proper_exceptions_with_args(self):
        try:
            RemoteOperation(RaiseOperation_Safe(), "localhost").run()
        except ExceptionWithArgs as e:
            self.assertTrue(e.x == 1 and e.y == 2)
        else:
            self.fail("RaiseOperation_Safe should have thrown ExceptionWithArgs(1, 2)")

    # It is crucial that the RMI is debuggable!
    def test_Remote_harden(self):
        """ Ensure that some logging occurs in event of error. """
        # One case encountered thus far is the raising of a pygresql DatabaseError,
        # which due to the import from a shared object (I think), does not behave
        # nicely in terms of imports and namespacing. """
        try:
            RemoteOperation(RaiseOperation_Unpicklable(), "localhost").run()
        except DatabaseError:
            pass
        else:
            self.fail("""A DatabaseError should have been raised remotely, and because it cannot
                         be pickled cleanly (due to a strange import in pickle.py),
                         an ExecutionError should have ultimately been caused.""")
            # TODO: Check logs on disk. With gplogfilter?

    def test_ParallelOperation_succeeds(self):
        ops = ParallelOperation([ListFiles("/tmp")], 1)
        ops.run()
        self.assertTrue(len(ops.operations[0].get_ret()) > 0)

    def test_ParallelOperation_handles_empty_operations_successfully(self):
        ParallelOperation([]).run()
        ParallelOperation([], 0).run()
        ops = ParallelOperation([], 1)
        ops.run()
        self.assertTrue(len(ops.operations) == 0)
        self.assertTrue(ops.parallelism == 0)

    def test_ParallelOperation_with_operation_but_no_threads_raises(self):
        with self.assertRaises(Exception):
            ParallelOperation([ListFiles("/tmp")], 0).run()

    @mock.patch('gppylib.commands.base.logger.debug')
    @mock.patch('pickle.loads')
    @mock.patch('gppylib.operations.utils.Command')
    @mock.patch('os.path.split', return_value = '/')
    def test_RemoteOperation_logger_debug(self, mock_split, mock_cmd, mock_lods, mock_debug):
        # We want to lock down the Command's get_results().stdout.
        cmd_instance = mock_cmd.return_value
        cmd_instance.get_results.return_value.stdout = 'output'

        mockRemoteOperation = RemoteOperation(operation=TestOperation(), host="sdw1", msg_ctx="dbid 2")
        mockRemoteOperation.execute()
        mock_debug.assert_has_calls([mock.call("Output for dbid 2 on host sdw1: output")])

if __name__ == '__main__':
    run_tests()
