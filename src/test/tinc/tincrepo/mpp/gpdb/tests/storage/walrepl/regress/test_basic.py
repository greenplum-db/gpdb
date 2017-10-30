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
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

import mpp.gpdb.tests.storage.walrepl.lib
import mpp.gpdb.tests.storage.walrepl.run

import os
import re
import subprocess
import socket
import time
import shutil
import sys
import signal

class regress(mpp.gpdb.tests.storage.walrepl.run.StandbyRunMixin, MPPTestCase):

    def run_sql(self, sql, port=os.environ.get('PGPORT', 5432)):
        return subprocess.Popen(['psql',
                                 '-c', sql,
                                 '-p', port],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)

    def test_tli_mismatch(self):
        """
        Test to verify if TLI mismatch issue during Pass 3 of xlog record
        (checkpoint) replay occurs or not. A set of checkpoints one after
        the other when replayed on the standby and then if the 
        standby is promoted, it should go through the prmotion just fine.

        The flow of this test is as follows.
        1. Initiate the Standby using the Master (primary) postmaster
           paramerters.
        2. Perform explicit checkpoints and wait so that they get replicated.
        3. Then promote the standby, wait and then try to access it.
        4. If we can successfully access it, its a Pass otherwise a Fail.
        """

        PSQL.run_sql_command('DROP table if exists foo')
        PSQL.run_sql_command('create table foo (a int)')
        PSQL.run_sql_command('insert into foo '
                             'select * from generate_series(1, 1000)')

        # Initial setup, forcibly remove standby and install new one
        pgutil = GpUtility()
        pgutil.remove_standby()   
        logger.info ('\nCreate a standby...')
        res = self.standby.create()
        self.assertEqual(res, 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # Wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        logger.info('Standby activated...')
        logger.info('Give the standby some time to catchup...')
        time.sleep(3)

        logger.info('Create checkpoints and let them get replicated...')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')
        PSQL.run_sql_command('checkpoint')

        time.sleep(2)

        # Promote standby
        # We don't kill/stop the primary, as it's convenient for
        # next testing
        logger.info('Promote the standby immediatly')
        self.standby.promote()

        logger.info('Wait for the standby to be ready to accept connections ...')
        PSQL.wait_for_database_up(port=self.standby.port)

        # Verify the result replicated to the standby.
        logger.info('Verify if table foo exists...')
        proc = self.run_sql('select count(*) from foo',
                            str(self.standby.port))

        # The table should exist
        (stdout, stderr) = proc.communicate()
        logger.info("stdout: %s" % stdout)
        logger.info("stderr: %s" % stderr)
        search = "1000"
        self.assertTrue(stdout.find(search) >= 0)

        logger.info('Pass')
