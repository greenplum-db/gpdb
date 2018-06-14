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
from gppylib.db import dbconn
from tinctest import logger
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.lib.filerep_util import Filerepe2e_Util

from mpp.gpdb.tests.storage.walrepl import lib as walrepl
import mpp.gpdb.tests.storage.walrepl.run

import os
import shutil
import subprocess

class basebackup_cases(mpp.gpdb.tests.storage.walrepl.run.StandbyRunMixin, MPPTestCase):

    def tearDown(self):
        super(basebackup_cases, self).tearDown()
        self.reset_fault('base_backup_post_create_checkpoint')

    def run_gp_inject_fault(self, fault_type, fault_name, s_occurrence=1, e_occurrence=1):
        fault_injector = Filerepe2e_Util()
        fault_injector.inject_fault(f=fault_name, y=fault_type, seg_id=1, s_o=s_occurrence, e_o=e_occurrence)

    def resume(self, fault_name):
        self.run_gp_inject_fault('resume', fault_name)

    def suspend_at(self, fault_name):
        self.run_gp_inject_fault('suspend', fault_name, 1, -1)

    def reset_fault(self, fault_name):
        self.run_gp_inject_fault('reset', fault_name)

    def test_xlogcleanup(self):
        """
        Test for verifying if xlog seg created while basebackup
        dumps out data does not get cleaned
        """

        shutil.rmtree('base', True)
        PSQL.run_sql_command('DROP table if exists foo')

        # Inject fault at post checkpoint create (basebackup)
        logger.info ('Injecting base_backup_post_create_checkpoint fault ...')
        self.suspend_at('base_backup_post_create_checkpoint')

        # Now execute basebackup. It will be blocked due to the
        # injected fault.
        logger.info ('Perform basebackup with xlog & recovery.conf...')
        pg_basebackup = subprocess.Popen(['pg_basebackup', '-x', '-R', '-D', 'base']
                                          , stdout = subprocess.PIPE
                                          , stderr = subprocess.PIPE)

        # Give basebackup a moment to reach the fault & 
        # trigger it
        logger.info('Check if suspend fault is hit ...')
        fault_injector = Filerepe2e_Util()
        triggered = fault_injector.check_fault_status(
            fault_name='base_backup_post_create_checkpoint',
            seg_id=1,
            status='triggered')
        self.assertTrue(triggered, 'Fault was not triggered')

        # Perform operations that causes xlog seg generation
        logger.info ('Performing xlog seg generation ...')
        count = 0
        while (count < 10):
            PSQL.run_sql_command('select pg_switch_xlog(); select pg_switch_xlog(); checkpoint;')
            count = count + 1

        # Resume basebackup
        self.resume('base_backup_post_create_checkpoint')

        # Wait until basebackup end
        logger.info('Waiting for basebackup to end ...')

        sql = "SELECT count(*) FROM pg_stat_replication"
        with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
            while (True):
                curs = dbconn.execSQL(conn, sql)
                results = curs.fetchall()

                if (int(results[0][0]) == 0):
                    break;

        # Verify if basebackup completed successfully
        # See if recovery.conf exists (Yes - Pass)
        self.assertTrue(os.path.exists(os.path.join('base','recovery.conf')))

        logger.info ('Found recovery.conf in the backup directory.')
        logger.info ('Pass')
