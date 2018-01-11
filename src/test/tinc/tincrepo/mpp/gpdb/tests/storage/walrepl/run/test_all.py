"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from gppylib.db import dbconn
from tinctest import TINCTestCase
from mpp.models import SQLTestCase

import mpp.gpdb.tests.storage.walrepl.lib
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin

import subprocess

class SQLwithStandby(StandbyRunMixin, SQLTestCase):
    """
    This test case is a template to peform SQL while standby is running.
    We don't connect to standby, but just see the primary's behavior.
    For standby's correctness, see promote test cases.
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'sql'
    ans_dir = 'ans'
    out_dir = 'output'

    def run_test(self):
        """
        Override SQLTestCase method.  Create a standby and run SQL.
        """
        sql_file = self.sql_file
        ans_file = self.ans_file
        self.assertEqual(self.standby.create(), 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        result = super(SQLwithStandby, self).run_test()
        return result

class ShutdownTestCase(StandbyRunMixin, TINCTestCase):

    def test_smartshutdown(self):
        """
        @description smartshutdown should be able to stop standby replay.
        @tags sanity
        """

        res = self.standby.create()
        self.assertEqual(res, 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        proc = subprocess.Popen(['pg_ctl', '-D', self.standby_datadir, 'stop'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()

        self.assertEqual(proc.returncode, 0)
        return True
