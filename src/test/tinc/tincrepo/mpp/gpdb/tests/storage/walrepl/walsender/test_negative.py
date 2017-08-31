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

from gppylib.gparray import GpArray
from gppylib.commands import gp
from gppylib.commands.base import Command
from gppylib.db import dbconn
from tinctest import logger
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin

import os
import re
import subprocess
import socket
import time
import shutil
import sys
import signal

class neg_test(StandbyRunMixin, MPPTestCase):

    def get_pid_having_keyword(self, keyword):
        proc = subprocess.Popen(['ps', '-ef'], stdout=subprocess.PIPE)
        stdout = proc.communicate()[0]
        search = keyword
        for line in stdout.split('\n'):
            if (line.find(search) > 0):
                split_line = re.split(r'\s+', line.strip())
                break

        self.assertTrue(len(split_line) > 0)
        pid = int(split_line[1])
        return pid

    def generate_trigger_file(self, content):
        filepath = os.path.join(self.standby.datadir, 'wal_rcv_test')
        self.assertTrue(content)

        with open(filepath, 'wb') as trigger_file:
            trigger_file.write(content)

    def count_blocked(self):
        sql = ("select count(*) from pg_stat_activity "
               "where waiting = 't' and waiting_reason = 'replication'")
        with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
            curs = dbconn.execSQL(conn, sql)
            results = curs.fetchall()

        return results[0][0]

    def test_negative(self):

        """
            This test verifies that if the WAL sender dies due to
            to any reason ,it should not keep backends blocked on
            replication waiting forever in sync-rep queue
        """

        # Verify if the database is up. Run some sql.
        logger.info ('Verify the DB is up...')
        PSQL.run_sql_command('DROP table if exists foo',dbname=os.environ.get('PGDATABASE'))
        Command('remove standby', 'gpinitstandby -ra').run()
        self.assertEqual(self.standby.create(), 0)

        # Trigger file cleanup
        if (os.path.exists(os.path.join(self.standby.datadir,
                                        'wal_rcv_test'))):
            os.remove(os.path.join(self.standby.datadir, 'wal_rcv_test'))

        # Setup a standby
        logger.info ('Setting up standby...')
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # Wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)
        logger.info('Activated WAL Receiver...')

        # Once the WAL receiver starts, signal it to suspend
        wal_rcv_pid = self.get_pid_having_keyword('wal receiver process')
        trigger_content = 'wait_before_send'
        logger.info('Suspending WAL Receiver(' + str(wal_rcv_pid) +') ' +
              'with ' + trigger_content + '...')

        self.generate_trigger_file(trigger_content)
        os.kill(wal_rcv_pid, signal.SIGUSR2)

        # Once suspended, spawn a new backend so that it gets blocked
        # Confirm that its blocked

        result = False
        for i in walrepl.polling(20, 1):
            sql = 'CREATE TABLE foo%s(a int, b int)' % str(i)
            subprocess.Popen(['psql', '-c', sql],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            num_blocked = self.count_blocked()
            if num_blocked >= 1:
                result = True
                break

        self.assertTrue(result, "no backend blocked")

        logger.info('The backend is blocked for replication (referred pg_stat_activity)...')

        logger.info('Shutdown the standby to break the connection with Master...')

        # Stop the standby as its of no use anymore
        self.standby.stop()

        # Verify that the wal sender died, max within 1 min
        for retry in range(1,30):
            num_walsender = self.count_walsender()
            if num_walsender == 0:
                break;
            logger.info('Wal sender still exists, retrying ...' + str(retry))
            time.sleep(2);

        self.assertEqual(num_walsender, 0, "WAL sender has not gone")

        logger.info('Wal sender is now dead...')

        # Confirm that the there are no more replication waiting backends
        # Thats the test
        num_blocked = self.count_blocked()
        self.assertEqual(num_blocked, 0,
                         str(num_blocked) + " backends still blocked")
        logger.info('The backend is no more blocked...')
