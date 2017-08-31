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

from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby
from mpp.gpdb.tests.storage.walrepl.lib import polling

import os
import subprocess
import socket
import time
import shutil
import sys
import signal
import cStringIO
import re

class smart_shutdown(MPPTestCase):

    def setUp(self):
        # Remove standby if present
        GpinitStandby().run(option='-r')
        super(smart_shutdown, self).setUp()

    def last_ckpt_lsn(self, pg_control_dump):

        lines  = pg_control_dump.splitlines()
        ckpt_lsn = None
        split_line = []
        for line in range(0, len(lines) - 1):
            if ((lines[line]).find("Latest checkpoint location") != -1):
                split_line = re.split(r'\s+',(lines[line]).strip())
                break

        self.assertTrue(len(split_line) > 0)
        return split_line[3]

    def last_mod_file(self, path):

        self.assertTrue(os.path.exists(path))

        max_mtime = 0
        max_file = ''
        for root, dirs, files in os.walk(path):
            for fname in files:
                full_path = os.path.join(path, fname)
                mtime = os.stat(full_path).st_mtime
                if mtime > max_mtime:
                    max_mtime = mtime
                    max_file = fname
        return (os.path.join(path,max_file));

    def count_walsender(self):
        sql = ("SELECT count(*) FROM pg_stat_replication")
        with dbconn.connect(dbconn.DbURL()) as conn:
            curs = dbconn.execSQL(conn, sql)
            results = curs.fetchall()

        return results[0][0]


    def test_smart_shutdown(self):
        # 1. Verify if the system is UP and there is no WAL Receiver running
        # 2. Perform basebackup and deploy it into some dest. directory
        # 3. Copy recover.conf into the dest. directory to be used by Standby
        # 4. Initiate the Standby using the Master (primary) postmaster
        #    paramerters
        # 5. Perform some transaction to generate xlog. Then do a smart shutdown
        # 6. Once the primary DB is down, find the last checkpoint  from pg_control
        #    on primary. Check the last modified xlog seg from the standby and find
        #    if the last checkpoint from primary exists
        # 7. It should be present there!

        # 0. Stop standby if it's running
        PSQL.run_sql_command('DROP table if exists foo')
        standby = Standby('base', 5433)
        standby.stop()

        # 1. Verify if the system is UP and there is no WAL sender running
        self.assertEqual(self.count_walsender(), 0)
        logger.info('No active WAL Receiver found')

        # 2. Perform basebackup and deploy it into some dest.
        #    (currenttly hardcoded 'base') directory
        shutil.rmtree('base', True)

        logger.info('Performing and deploying base backup ...')
        standby.create()

        # 3.Copy recover.conf into the dest. directory to be used by StandBy
        logger.info('Deploying recovery.conf...')

        # 4. Initiate the StandBy using the Master (primary) postmaster
        #    paramerters
        logger.info('Initiating Standby...')
        res = standby.start()
        self.assertTrue(res.wasSuccessful())

        num_walsender = 0
        for i in polling(10, 0.5):
            num_walsender = self.count_walsender()
            if num_walsender > 0:
                break
        self.assertEqual(num_walsender, 1)

        logger.info('Activated WAL Receiver...')

        # 5. Perform some transaction to generate xlog. Then do a smart shutdown
        logger.info('Perform some transaction to generate some XLOG')
        PSQL.run_sql_command('Create table foo (a int)')

        logger.info('Now perform smart shutdown (gpstop -a)')
        cmd = Command(name="gpstop smart",
                      cmdStr="source %s/greenplum_path.sh;\
                      gpstop -a" % os.environ["GPHOME"])
        cmd.run(validateAfter=True)

        # 6. Once the primary DB is down, find the last checkpoint  from pg_control
        #    on primary. Check the last modified xlog seg from the standby and find
        #    if the last checkpoint from primary exists
        logger.info('Read the pg_control from primary, find the last checkpoint & see if it made to standby')

        standby_xlog_path = os.path.join('base','pg_xlog')

        cmd= Command(name = 'pg_controldata ' + os.environ.get('MASTER_DATA_DIRECTORY'),
                     cmdStr = 'pg_controldata ' + os.environ.get('MASTER_DATA_DIRECTORY'))
        cmd.run(validateAfter=True);

        primary_last_ckpt_lsn = self.last_ckpt_lsn((cmd.get_results()).stdout)

        logger.info ("Primary last checkpoint LSN = " + primary_last_ckpt_lsn)

        standby_last_mod_xlog = self.last_mod_file(standby_xlog_path)
        logger.info ("Last mod standby XLOG = " + standby_last_mod_xlog )

        cmd= Command(name = 'xlogdump standby last modifiled xlog',
                     cmdStr="xlogdump " + standby_last_mod_xlog)
        cmd.run(validateAfter=True);

        logger.info('See if we find the shutdown LSN in the XLOG seg file')
        lines  = (cmd.get_results()).stdout.splitlines()
        flag = False
        for line in range(0, len(lines) - 1):
            if ((lines[line]).find(primary_last_ckpt_lsn) > -1):
                self.assertTrue((lines[line]).find("checkpoint") > -1)
                self.assertTrue((lines[line]).find("shutdown") > -1)
                flag = True
                break

        self.assertTrue(flag)
        logger.info('PASS')

        # Re-start the database
        logger.info('Now restart the DB (gpstart -a)')
        cmd = Command(name="gpstop smart",
                      cmdStr="source %s/greenplum_path.sh;\
                      gpstart -a" % os.environ["GPHOME"])
        cmd.run(validateAfter=True)

        # Cleanup. Currently we dont have a clean way of WAL rcv dying
        logger.info('Kill the standby processes as clean standby killing is not supported')
        cmd = Command(name="kill standby",
                      cmdStr="kill -9 `ps -ef | grep 5433 | grep -v grep | awk '{print $2}'`")
        cmd.run(validateAfter=True)
