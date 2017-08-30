#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import unittest
from gppylib.commands.base import Command, WorkerPool, RemoteExecutionContext, GPHOME, LocalExecutionContext
from mock import patch


class WorkerPoolTestCase(unittest.TestCase):

    def tearDown(self):
        Command.propagate_env_map.clear()

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

    def test_RemoteExecutionContext_uses_default_gphome(self):
        self.subject = RemoteExecutionContext("myhost", "my_stdin")
        cmd = Command("dummy name", "echo 'foo'")
        self.subject.execute(cmd)
        self.assertIn(". %s/greenplum_path.sh;" % GPHOME, cmd.cmdStr)

    def test_RemoteExecutionContext_uses_provided_gphome_when_set(self):
        self.subject = RemoteExecutionContext(targetHost="myhost", stdin="my_stdin", gphome="other/gphome")
        cmd = Command("dummy name", "echo 'foo'")
        self.subject.execute(cmd)
        self.assertIn(". other/gphome/greenplum_path.sh;", cmd.cmdStr)

    def test_LocalExecutionContext_uses_no_environment(self):
        self.subject = LocalExecutionContext(None)
        cmd = Command('test', cmdStr='ls /tmp')
        self.subject.execute(cmd)
        self.assertEquals("ls /tmp", cmd.cmdStr)

    def test_LocalExecutionContext_uses_ampersand(self):
        self.subject = LocalExecutionContext(None)
        cmd = Command('test', cmdStr='ls /tmp')
        cmd.propagate_env_map['foo'] = 1
        self.subject.execute(cmd)
        self.assertEquals("foo=1 && ls /tmp", cmd.cmdStr)

    def test_LocalExecutionContext_uses_ampersand_multiple(self):
        self.subject = LocalExecutionContext(None)
        cmd = Command('test', cmdStr='ls /tmp')
        cmd.propagate_env_map['foo'] = 1
        cmd.propagate_env_map['bar'] = 1
        self.subject.execute(cmd)
        self.assertEquals("bar=1 && foo=1 && ls /tmp", cmd.cmdStr)

    def test_RemoteExecutionContext_uses_ampersand_multiple(self):
        self.subject = RemoteExecutionContext('localhost', None, 'gphome')
        cmd = Command('test', cmdStr='ls /tmp')
        cmd.propagate_env_map['foo'] = 1
        cmd.propagate_env_map['bar'] = 1
        self.subject.execute(cmd)
        self.assertEquals("bar=1 && foo=1 && ssh -o \'StrictHostKeyChecking no\' localhost "
                          "\". gphome/greenplum_path.sh; bar=1 && foo=1 && ls /tmp\"", cmd.cmdStr)

    def test_no_workders_in_WorkerPool(self):
        with self.assertRaises(Exception):
            WorkerPool(numWorkers=0)
