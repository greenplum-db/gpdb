#!/usr/bin/env python

import unittest

from gppylib.operations.filespace import FileType 
from gppylib.commands.base import Command, WorkerPool

class ConcurrentFilespaceMoveTestCase(unittest.TestCase):
    """ This test suite tests the scenario of running gpfilespace concurrently while
        trying to move the filespace. 
        The expected behavior is that only one of the processes succeeds and the 
        rest error out."""

    ALREADY_RUNNING_MSG = 'Another instance of gpfilespace is already running!'

    def setUp(self):
        self.pool = None
        self.pool = WorkerPool()

    def tearDown(self):
        if self.pool:
            self.pool.haltWork()
            self.pool.joinWorkers()
            self.pool.join()

    def get_move_filespace_cmd(self, filespace='myfspc', file_type=FileType.TEMPORARY_FILES):
        if file_type == FileType.TEMPORARY_FILES:
            file_type = 'movetempfiles'
        elif file_type == FileType.TRANSACTION_FILES:
            file_type = 'movetransfiles'

        return Command(name='move filespace', cmdStr='gpfilespace --%s %s' % (file_type, filespace))

    def run_concurrently(self, cmd_list):

        for cmd in cmd_list:
            self.pool.addCommand(cmd)
        self.pool.join()

    def check_concurrent_execution_result(self, execution_results):

        succeeded = 0
        for cmd in execution_results:
            results = cmd.get_results().stdout.strip()
            if self.ALREADY_RUNNING_MSG in results:
                continue
            succeeded += 1

        self.assertEqual(succeeded, 1)
            
    def test00_move_temp_filespace(self):

        cmd_list = [self.get_move_filespace_cmd(file_type=FileType.TEMPORARY_FILES) for i in range(2)]
        self.run_concurrently(cmd_list)
        self.check_concurrent_execution_result(self.pool.getCompletedItems())
            
    def test01_move_trans_filespace(self):

        cmd_list = [self.get_move_filespace_cmd(file_type=FileType.TRANSACTION_FILES) for i in range(2)]
        self.run_concurrently(cmd_list)
        self.check_concurrent_execution_result(self.pool.getCompletedItems())

    def test02_move_temp_and_trans_filespace(self):
        
        cmd_list = [self.get_move_filespace_cmd(file_type=FileType.TEMPORARY_FILES), self.get_move_filespace_cmd(file_type=FileType.TRANSACTION_FILES)]
        self.run_concurrently(cmd_list) 
        self.check_concurrent_execution_result(self.pool.getCompletedItems())
