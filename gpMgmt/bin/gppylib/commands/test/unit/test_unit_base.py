#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved. 
#

import unittest2 as unittest
from gppylib.commands.base import Command, WorkerPool
from mock import patch

class WorkerPoolTestCase(unittest.TestCase):

    @patch('gppylib.commands.base.gplog.get_default_logger')
    def test_print_progress(self, mock1):
        w = WorkerPool(numWorkers=32)
        c1 = Command('dummy command1', '')
        c2 = Command('dummy command2', '')
        w.addCommand(c1)
        w.addCommand(c2)
        w.join()
        w.print_progress(2)
        self.assertTrue(mock1.called_with('100.00% of jobs completed'))
        w.haltWork()

    @patch('gppylib.commands.base.gplog.get_default_logger')
    def test_print_progress_none(self, mock1):
        w = WorkerPool(numWorkers=32)
        w.print_progress(0)
        w.join()
        self.assertTrue(mock1.called_with('0.00% of jobs completed'))
        w.haltWork()
