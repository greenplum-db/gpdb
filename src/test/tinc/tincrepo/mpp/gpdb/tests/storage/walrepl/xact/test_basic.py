#!/usr/bin/env python

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

from gppylib.commands.base import Command
from tinctest import logger
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase

from mpp.gpdb.tests.storage.walrepl import lib as walrepl
import mpp.gpdb.tests.storage.walrepl.run

from mpp.gpdb.tests.storage.walrepl.lib.fault import *

import os
import re
import subprocess
import socket
import time
import shutil
import sys
import signal

class xansrep(mpp.gpdb.tests.storage.walrepl.run.StandbyRunMixin, MPPTestCase):

    def tearDown(self):
        self.standby.remove_catalog_standby("")
        super(xansrep, self).tearDown()
        #self.reset_fault('all')
        Gpfault().reset_fault('transaction_abort_after_distributed_prepared')
        Gpfault().reset_fault('dtm_broadcast_commit_prepared')

    def run_sql(self, sql, port=os.environ.get('PGPORT', 5432)):
        return subprocess.Popen(['psql',
                                 '-c', sql,
                                 '-p', port],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)

    def test_xansrep(self):
        """
        Test for distributed transactions.  Here are two cases:
          A. a transaction in prepared state
          B. a transaction after broadcast commit prepared

        In case A, the transaction will be aborted, and B should be visible.

        The flow of this test is as follows.
        1. Initiate the Standby using the Master (primary) postmaster
           paramerters.
        2. B: Inject the fault to suspend Master after Commit done.
        3. B: Now execute a transaction and commit it. This master will be blocked.
        4. A: Inject the fault to suspend Master after Prepare done.
        5. A: Now execute a transaction and commit it. This transaction will be blocked.
        6. Promote the standby.
        7. Verify the result, transaction A results should not be visible and
           transaction B results should be visible.
        """

        PSQL.run_sql_command('DROP table if exists xansrep_prepare')
        PSQL.run_sql_command('DROP table if exists xansrep_commit')
        PSQL.run_sql_command('DROP table if exists xansrep_dummy')
        PSQL.run_sql_command('create table xansrep_dummy (a int)')
        PSQL.run_sql_command('insert into xansrep_dummy '
                             'select * from generate_series(1, 1000)')

        Command('remove standby', 'gpinitstandby -ra').run()
	fault = Gpfault()

        # 1. Initial setup
        res = self.standby.create()
        self.assertEqual(res, 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        # 2. Inject fault at commit prepared state
        result = fault.suspend_at('dtm_broadcast_commit_prepared')
        logger.info(result.stdout)
        self.assertEqual(result.rc, 0, result.stdout)

        # 3. Now execute a transaction and commit it. The backend is expected
        #    to be blocked.
        logger.info('Create table xansrep_commit...')

        # Due to the suspend, we don't wait for the result
        proc = self.run_sql('create table xansrep_commit as '
                            'select * from xansrep_dummy')

        logger.info('Check if suspend fault is hit after commit...')
        triggered = fault.wait_triggered('dtm_broadcast_commit_prepared')
        self.assertTrue(triggered, 'Fault was not triggered')

        # 4. Inject fault at prepared state
        result = fault.suspend_at(
                    'transaction_abort_after_distributed_prepared')
        logger.info(result.stdout)
        self.assertEqual(result.rc, 0, result.stdout)

        # 5. Now execute a transaction and commit it. The backend is expected
        #    to be blocked.
        logger.info('Create table xansrep_prepare...')

        # Due to the suspend, we don't wait for the result
        proc = self.run_sql('create table xansrep_prepare (a int)')

        logger.info('Check if suspend fault is hit ...')
        triggered = fault.wait_triggered(
                    'transaction_abort_after_distributed_prepared')
        self.assertTrue(triggered, 'Fault was not triggered')

        # 6. Promote standby
        # We don't kill/stop the primary, as it's convenient for
        # next testing, and the outcome wouldn't change.
        self.standby.promote()

        # 7. Verify the result replicated to the standby.
        logger.info('Verify if table xansrep_prepare exists...')
        proc = self.run_sql('select * from xansrep_prepare',
                            str(self.standby.port))
        # the table should not exist
        stderr = proc.communicate()[1]
        logger.info(stderr)
        search = "ERROR:  relation \"xansrep_prepare\" does not exist"
        self.assertTrue(stderr.find(search) >= 0)

        logger.info('Verify if table xansrep_commit exists...')
        proc = self.run_sql('select count(*) from xansrep_commit',
                            str(self.standby.port))
        # the table should exit
        stdout = proc.communicate()[0]
        logger.info(stdout)
        search = "1000"
        self.assertTrue(stdout.find(search) >= 0)

        logger.info('Pass')

