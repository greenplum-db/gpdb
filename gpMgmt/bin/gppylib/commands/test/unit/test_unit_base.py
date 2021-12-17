#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import unittest
from gppylib.commands.base import Command, WorkerPool, RemoteExecutionContext, GPHOME, LocalExecutionContext


class WorkerPoolTestCase(unittest.TestCase):

    def tearDown(self):
        Command.propagate_env_map.clear()

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
        self.assertEqual("ls /tmp", cmd.cmdStr)

    def test_LocalExecutionContext_uses_ampersand(self):
        self.subject = LocalExecutionContext(None)
        cmd = Command('test', cmdStr='ls /tmp')
        cmd.propagate_env_map['foo'] = 1
        self.subject.execute(cmd)
        self.assertEqual("foo=1 && ls /tmp", cmd.cmdStr)

    def test_LocalExecutionContext_uses_ampersand_multiple(self):
        self.subject = LocalExecutionContext(None)
        cmd = Command('test', cmdStr='ls /tmp')
        cmd.propagate_env_map['foo'] = 1
        cmd.propagate_env_map['bar'] = 1
        self.subject.execute(cmd)
        self.assertEqual("bar=1 && foo=1 && ls /tmp", cmd.cmdStr)

    def test_RemoteExecutionContext_uses_ampersand_multiple(self):
        self.subject = RemoteExecutionContext('localhost', None, 'gphome')
        cmd = Command('test', cmdStr='ls /tmp')
        cmd.propagate_env_map['foo'] = 1
        cmd.propagate_env_map['bar'] = 1
        self.subject.execute(cmd)
        self.assertEqual("bar=1 && foo=1 && ssh -o StrictHostKeyChecking=no -o ServerAliveInterval=60 localhost "
                          "\". gphome/greenplum_path.sh; bar=1 && foo=1 && ls /tmp\"", cmd.cmdStr)
