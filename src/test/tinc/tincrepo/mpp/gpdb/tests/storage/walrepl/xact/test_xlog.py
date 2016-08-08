#!/usr/bin/env python

"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

class xlog(mpp.gpdb.tests.storage.walrepl.run.StandbyRunMixin, MPPTestCase):

    def tearDown(self):
        Gpfault().reset_fault('transaction_abort_after_distributed_prepared')

    def check_pg_stat_activity(self, query):
        """
        Check if the transaction has been started for pg_stat_activity
        """
        logger.info('checking for query in pg_stat_activity')
        MAX_TRIES = 300
        tries = 0
        while tries < MAX_TRIES:
            output = PSQL.run_sql_command('SELECT * FROM pg_stat_activity')
            if query in output:
                logger.info('found query in pg_stat_activity')
                return True 
            tries += 1
        return False

    def test_xlogPreparedXactSeg(self):
        """
        Test to verify the xlog on segment gets cleaned-up only till point of oldest prepared transaction.

        The flow of this test is as follows.
        1. Initiate the Standby using the Master (primary) postmaster
           paramerters.
        2. A: Inject the fault to suspend Mater after Prepare done.
        3. A: Now execute a transaction and commit it. This transaction will be blocked.
        4. B: Inject the fault to suspend Mater after Commit done.
        5. B: Now execute a transaction and commit it. This master will be blocked.
        6. Promote the standby.
        7. Verify the result, transaction A results should not be visible and
           transaction B results should be visible.
        """

        PSQL.run_sql_command('DROP table if exists xansrep_prepare')
        PSQL.run_sql_command('DROP table if exists xansrep1, xansrep2, xansrep3, xansrep4')
	fault = Gpfault()

        # 2. Inject fault at prepared state
        result = fault.suspend_at(
                    'transaction_abort_after_distributed_prepared')
        logger.info(result.stdout)
        self.assertEqual(result.rc, 0, result.stdout)

        # 3. Now execute a transaction and commit it. The backend is expected
        #    be blocked.
        logger.info('Create table xansrep_prepare...')
        
        create_xansprep_prepare_query = 'create table xansprep_prepare (a int)'
        # Due to the suspend, we don't wait for the result
        subprocess.Popen(['psql',
                                 '-c', create_xansprep_prepare_query,
                                 '-p', os.environ.get('PGPORT')],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)

        logger.info('Check if suspend fault is hit ...')
        if not self.check_pg_stat_activity(create_xansprep_prepare_query):
            logger.error('transaction has not been started yet')
        triggered = fault.wait_triggered(
                    'transaction_abort_after_distributed_prepared')
        self.assertTrue(triggered, 'Fault was not triggered')

	# Lets trigger switch to xlog on segment
	PSQL.run_sql_command_utility_mode(sql_cmd='select pg_switch_xlog()', port=2100)
	PSQL.run_sql_command_utility_mode(sql_cmd='checkpoint', port=2100)

	# Generate more records on xlog
	PSQL.run_sql_command('create table xansrep1 (a int)')
	PSQL.run_sql_command('create table xansrep2 (a int)')
	PSQL.run_sql_command_utility_mode(sql_cmd='select pg_switch_xlog()', port=2100)
	PSQL.run_sql_command_utility_mode(sql_cmd='checkpoint', port=2100)

	PSQL.run_sql_command('create table xansrep3 (a int)')
	PSQL.run_sql_command('create table xansrep4 (a int)')
	PSQL.run_sql_command_utility_mode(sql_cmd='select pg_switch_xlog()', port=2100)
	PSQL.run_sql_command_utility_mode(sql_cmd='checkpoint', port=2100)

	cmd = Command('gpstop', 'gpstop -air')
	cmd.run(validateAfter=True)

