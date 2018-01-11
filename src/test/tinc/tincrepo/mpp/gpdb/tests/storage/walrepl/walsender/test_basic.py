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
from mpp.gpdb.tests.storage.walrepl.lib.walcomm import *
from mpp.gpdb.tests.storage.walrepl.lib import PgControlData

import subprocess

class simple(MPPTestCase):

    def setUp(self):
        Command('gpinitstandby -ra', 'gpinitstandby -ra').run()

    def tearDown(self):
        pass

    def test_identify_system(self):
        """
        Check if the system identifier matches.

        @tags sanity
        """

        with WalClient("replication=true") as client:
            (sysid, tli, xpos) = client.identify_system()

            datadir = os.environ['MASTER_DATA_DIRECTORY']
            controldata = PgControlData(datadir)

            assert sysid == controldata.get('Database system identifier')

    def test_streaming(self):
        """
        Run sendtest, let database emit WAL.
        sendtest should receive a new WAL records.  After all, we kill
        the walsender process, otherwise the test doesn't finish.

        @tags sanity
        """

        PSQL.run_sql_command('DROP TABLE IF EXISTS foo')
        with WalClient("replication=true") as client:
            (sysid, tli, xpos) = client.identify_system()

            xpos_ptr = XLogRecPtr.from_string(xpos)
            client.start_replication(xpos_ptr)

            # Can't use PSQL here as it is blocked due to Sync Rep
            subprocess.Popen(['psql', '-c', 'CREATE TABLE foo(a int, b int)'],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            while True:
                msg = client.receive(1000)
                if isinstance(msg, WalMessageData):
                    header = msg.header
                    # sync replication needs a reply otherwise backend blocks
                    client.reply(header.walEnd, header.walEnd, header.walEnd)
                    # success, should get some 'w' message
                    break
                elif isinstance(msg, WalMessageNoData):
                    # could be timeout
                    client.reply(xpos_ptr, xpos_ptr, xpos_ptr)
                else:
                    raise StandardError(msg.errmsg)

    def test_async(self):
        """
        Run sendtest in async mode.

        @tags sanity
        """

        PSQL.run_sql_command('DROP TABLE IF EXISTS foo')
        with WalClient("replication=true") as client:
            self.assertEqual(client.status(), CONNECTION_OK)
            (sysid, tli, xpos) = client.identify_system()

            xpos_ptr = XLogRecPtr.from_string(xpos)
            client.start_replication(xpos_ptr, sync=False)

            # wouldn't block since it's async connection.
            PSQL.run_sql_command('CREATE TABLE foo(a int, b int)')

            # it may take time to get complete message
            for i in walrepl.polling(10, 0.5):
                msg = client.receive(1000)
                if isinstance(msg, WalMessageData):
                    break
            self.assertIsInstance(msg, WalMessageData)

    def test_invalid_command_new(self):
        """
        Check syntax error.

        @tags sanity
        @product_version gpdb: [4.3.6.2-4.3.9.0]
        """

        with WalClient("replication=true") as client:
            # connection established?
            self.assertEqual(client.status(), CONNECTION_OK)

            # send bad message
            res = client.execute("IDENTIFY_SYSTEM FOO")
            # check if server responded syntax error
            self.assertEqual(res.status(), PGRES_FATAL_ERROR)
            self.assertIn("syntax error", client.error_message())
            self.assertEqual(client.status(), CONNECTION_OK)
        with WalClient("replication=true") as client:
            self.assertEquals(client.status(), CONNECTION_OK)

            # send bad message
            res = client.execute("START_REPLICATION")
            # check if server responded syntax error
            self.assertEqual(res.status(), PGRES_FATAL_ERROR)
            self.assertIn("syntax error", client.error_message())
            self.assertEqual(client.status(), CONNECTION_OK)
