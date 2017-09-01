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

from tinctest import logger
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from gppylib.db import dbconn

from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage import walrepl
from mpp.gpdb.tests.storage.walrepl.lib.walcomm import *
from mpp.gpdb.tests.storage.walrepl.lib import PgControlData
from gppylib.commands.base import Command

import os
import re
import select
import signal
import subprocess
import time
import sys

class syncrep(StandbyRunMixin, MPPTestCase):

    def generate_trigger_file(self, content):
        filename = 'wal_rcv_test'
        self.assertTrue(content is not None)

        filepath = os.path.join(self.standby.datadir, filename)
        with open(filepath, 'wb') as f:
            f.write(content)

    def wait_stdout(self, proc, timeout):
        rlist = [proc.stdout.fileno()]
        (rout, _, _) = select.select(rlist, [], [], timeout)

        return len(rout) > 0

    def set_guc(self, guc_name, guc_value):

        logger.info('Configuring ' + guc_name +' ...')
        cmd = Command("gpconfig " + guc_name,
                          "gpconfig -c " + guc_name + " -v " + guc_value)
        cmd.run()
        self.assertEqual(cmd.get_results().rc, 0, str(cmd))

        logger.info('gpstop -u to reload config files...')
        cmd = Command("gpstop -u",
                          "gpstop -u")
        cmd.run()
        self.assertEqual(cmd.get_results().rc, 0, str(cmd))
 

    def test_syncrep(self):

        # 1. Initiate the Standby
        # 2. Once the WAL receiver starts, signal it to suspend post xlog flush
        #    but before sending the ack.
        # 3. Now execute a transaction and commit it. The backend is expected
        #    be blocked.
        # 4. Resume the WALReceiver and see the transaction passed and its
        #    results are visible.

        # cleanup
        PSQL.run_sql_command('DROP table if exists foo')

        # 1. create standby and start
        res = self.standby.create()
        self.assertEqual(res, 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        # 2. Once the WAL receiver starts, signal it to suspend post xlog flush
        #    but before sending the ack.
        proc = subprocess.Popen(['ps', '-ef'], stdout=subprocess.PIPE)
        stdout = proc.communicate()[0]
        search = "wal receiver process"
        for line in stdout.split('\n'):
            if (line.find(search) > 0):
                split_line = re.split(r'\s+', line.strip())
                break

        self.assertTrue(len(split_line) > 0)

        wal_rcv_pid = int(split_line[1])
        logger.info('Suspending WAL Receiver(' + str(wal_rcv_pid) +')...')
        self.generate_trigger_file('wait_before_send_ack')
        os.kill(wal_rcv_pid, signal.SIGUSR2)

        # 3. Now execute a transaction and commit it. The backend is expected
        #    be blocked.
        logger.info('Create table foo...')

        # we use subprocess since we expect it'll be blocked.
        proc = subprocess.Popen(['psql', '-c', 'create table foo (a int)'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        readable = self.wait_stdout(proc, 5.0)
        self.assertFalse(readable, 'psql did not block')

        # 4. Resume the WALReceiver and see the transaction passed and its
        #    results are visible.
        logger.info('Resume the WAL Receiver...')
        self.generate_trigger_file('resume')
        os.kill(wal_rcv_pid, signal.SIGUSR2)

        readable = self.wait_stdout(proc, 5.0)
        self.assertTrue(readable, 'psql still blocks')
        proc.communicate()

        logger.info('No blocked backend found!')

        logger.info('Verifying if table exists ? ...')
        PSQL(sql_cmd='select * from foo').run(validateAfter=True)

        logger.info('Pass')

    def test_unblock_while_catchup_out_of_range(self):
        """
        This test verifies if a backend gets blocked in case
        the WAL sender is still in catchup mode.
        """
        with WalClient("replication=true") as client:
            (sysid, tli, xpos) = client.identify_system()

            sql_startup = "SELECT count(*) FROM pg_stat_replication where state = 'startup'"
            with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                curs = dbconn.execSQL(conn, sql_startup)
                results = curs.fetchall()
                self.assertEqual(int(results[0][0]), 1, "No WAL sender in startup phase found")
            logger.info('WAL sender is alive and now is in startup phase...')

            # Generate enough xlog in WAL sender startup phase. None of the sql statements
            # should get blocked. If blocked we have some issue.
            # Checkpointing causes full page writes on updates/inserts. Hence helps
            # xlog generation.
            i = 0
            logger.info('Running a bunch of SQLs to generate enough xlog to maintain catchup phase...')
            while (i < 10):
                PSQL.run_sql_command('DROP TABLE IF EXISTS foo; CREATE TABLE foo(a int, b int); CHECKPOINT;')
                i = i + 1

            logger.info('Pass - Database does not block if WAL sender is alive and in startup phase')

            logger.info('Creating some xlog seg files to simulate catchup out-of-range..')
            i = 0
            while(i < 3):
                PSQL.run_sql_command('select pg_switch_xlog();select pg_switch_xlog();checkpoint;')
                i = i + 1

            xpos_ptr = XLogRecPtr.from_string(xpos)
            client.start_replication(xpos_ptr)

            while True:
                msg = client.receive(1000)

                if isinstance(msg, WalMessageData):
                    header = msg.header

                    # walsender must be still in catchup phase as a lot xlog needs to be sent
                    sql_catchup = "SELECT count(*) FROM pg_stat_replication where state = 'catchup'"
                    sql_table_present = "SELECT count(*) from pg_class where relname = 'foo'"
                    sql_bkd_count = ("SELECT count(*) from pg_stat_activity where waiting ='t' and waiting_reason = 'replication'")

                    with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                        curs = dbconn.execSQL(conn, sql_catchup)
                        results = curs.fetchall()
                        self.assertEqual(int(results[0][0]), 1, "No Catchup WAL sender found")
                        logger.info('WAL sender is alive and now is in catchup phase...')

                    logger.info('In catchup phase, run some sql...')
                    PSQL.run_sql_command('DROP TABLE IF EXISTS foo; CREATE TABLE foo(a int, b int);'
                                          ,dbname='postgres')

                    while (i < 5):
                        with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                            # verify if the previous backend is blocked
                            curs = dbconn.execSQL(conn, sql_bkd_count)
                            results = curs.fetchall()
                            if (int(results[0][0]) > 0):
                                self.assertTrue(0, "Previous backend was blocked ...")
                        i = i + 1
                    logger.info('Create table is NOT blocked...')

                    with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                        # verify if WAL sender is still in catchup phase
                        curs = dbconn.execSQL(conn, sql_catchup)
                        results = curs.fetchall()
                        self.assertEqual(int(results[0][0]), 1,
                        "WAL sender catchup phase over before verification")
                    logger.info('WAL sender is alive and still in catchup phase...')

                    with dbconn.connect(dbconn.DbURL(dbname='postgres'), utility=True) as conn:
                        # verify if WAL sender is still in catchup phase
                        curs = dbconn.execSQL(conn, sql_table_present)
                        results = curs.fetchall()
                        self.assertEqual(int(results[0][0]), 1, "Table foo not found")

                    # sync replication needs a reply otherwise backend blocks
                    client.reply(header.walEnd, header.walEnd, header.walEnd)
                    # success, should get some 'w' message
                    logger.info ("Pass - Database does not block if WAL sender is alive and "
                                  "the catchup is out-of-range")
                    break

                elif isinstance(msg, WalMessageNoData):
                    # could be timeout
                    client.reply(xpos_ptr, xpos_ptr, xpos_ptr)
                else:
                    raise StandardError(msg.errmsg)

    def test_block_while_catchup_within_range(self):

        """
        This test verifies if a backend gets blocked in case
        the WAL sender is still in catchup mode.
        """

        with WalClient("replication=true") as client:

            (sysid, tli, xpos) = client.identify_system()

            # Set the guc to > 1 so that we can verify the test
            # using less amount of xlog
            self.set_guc('repl_catchup_within_range', '3')

            # Generate enough xlog in WAL sender startup phase. None of the sql statements
            # should get blocked. If blocked we have some issue.
            # Checkpointing causes full page writes on updates/inserts. Hence helps
            # xlog generation.
            i = 0
            logger.info('Running a bunch of SQLs to generate enough xlog to maintain catchup phase...')
            while (i < 10):
                PSQL.run_sql_command('DROP TABLE IF EXISTS foo; CREATE TABLE foo(a int, b int); CHECKPOINT;')
                i = i + 1

            xpos_ptr = XLogRecPtr.from_string(xpos)
            client.start_replication(xpos_ptr)

            while True:
                msg = client.receive(1000)

                if isinstance(msg, WalMessageData):
                    header = msg.header

                    # walsender must be still in catchup phase as a lot xlog needs to be sent
                    sql_catchup = "SELECT count(*) FROM pg_stat_replication where state = 'catchup'"
                    sql_table_present = "SELECT count(*) from pg_class where relname = 'foo'"
                    sql_bkd_count = ("SELECT count(*) from pg_stat_activity where waiting ='t' and waiting_reason = 'replication'")

                    with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                        curs = dbconn.execSQL(conn, sql_catchup)
                        results = curs.fetchall()
                        self.assertEqual(int(results[0][0]), 1, "No Catchup WAL sender found")
                        logger.info('WAL sender is alive and now is in catchup phase...')
 
                    logger.info('In catchup phase, create table...')
                    subprocess.Popen(['psql', '-c',
                                      'DROP TABLE IF EXISTS raghav; create table raghav (a int);'],
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)


                    with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                        # verify if WAL sender is still in catchup phase
                        curs = dbconn.execSQL(conn, sql_catchup)
                        results = curs.fetchall()
                        self.assertEqual(int(results[0][0]), 1,
                        "WAL sender catchup phase over before verification")
                        logger.info('WAL sender is alive, still in catchup phase ..')

                    while (i < 5):
                        with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                            # verify if the previous backend is blocked
                            curs = dbconn.execSQL(conn, sql_bkd_count)
                            results = curs.fetchall()
                            if (int(results[0][0]) == 1):
                                break;
                            if (i == 4):
                                self.assertTrue(0, "Previous backend not blocked ...")
                        i = i + 1
                    logger.info('But, create table is blocked...')

                    with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
                        # verify if WAL sender is still in catchup phase
                        curs = dbconn.execSQL(conn, sql_catchup)
                        results = curs.fetchall()
                        self.assertEqual(int(results[0][0]), 1,
                        "WAL sender catchup phase over before verification")
                        logger.info('WAL sender is alive, in catchup phase and backend is blocked...')

                    # sync replication needs a reply otherwise backend blocks
                    client.reply(header.walEnd, header.walEnd, header.walEnd)
                    # success, should get some 'w' message
                    logger.info ("Pass - Backends block if WAL sender is alive and the catchup is within-range")
                    break

                elif isinstance(msg, WalMessageNoData):
                    # could be timeout
                    client.reply(xpos_ptr, xpos_ptr, xpos_ptr)
                else:
                    raise StandardError(msg.errmsg)

        logger.info ("Pass")
        self.set_guc('repl_catchup_within_range', '1')
