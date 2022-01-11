#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2012. All Rights Reserved.
#

import unittest
from gppylib.commands.base import Command, WorkerPool, RemoteExecutionContext, GPHOME, LocalExecutionContext,\
    set_cmd_results


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


class SetCmdResultsTestCase(unittest.TestCase):

    def _assert_cmd_passed(self, cmd):
        self.assertEqual(0, cmd.get_results().rc)
        self.assertEqual('', cmd.get_results().stdout)
        self.assertEqual('', cmd.get_results().stderr)
        self.assertEqual(True, cmd.get_results().completed)
        self.assertEqual(False, cmd.get_results().halt)
        self.assertEqual(True, cmd.get_results().wasSuccessful())

    def _assert_cmd_failed(self, cmd, expected_stderr='running the cmd failed'):
        self.assertEqual(1, cmd.get_results().rc)
        self.assertEqual('', cmd.get_results().stdout)
        self.assertTrue(expected_stderr in cmd.get_results().stderr)
        self.assertEqual(True, cmd.get_results().completed)
        self.assertEqual(False, cmd.get_results().halt)
        self.assertEqual(False, cmd.get_results().wasSuccessful())

    def test_set_cmd_results_no_exception(self):
        @set_cmd_results
        def test_decorator(cmd):
            cmd.name = 'new name'

        test_cmd = Command(name='original name', cmdStr='echo foo')
        test_decorator(test_cmd)
        self.assertEqual('new name', test_cmd.name)
        self._assert_cmd_passed(test_cmd)

    def test_set_cmd_results_catch_exception(self):
        @set_cmd_results
        def test_decorator(cmd):
            cmd.name = 'new name'
            raise Exception('running the cmd failed')

        test_cmd = Command(name='original name', cmdStr='echo foo')
        test_decorator(test_cmd)
        self.assertEqual('new name', test_cmd.name)
        self._assert_cmd_failed(test_cmd)
