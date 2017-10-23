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
import unittest2 as unittest

from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.walreceiver import GPLibPQTestCase

gputil = GpUtility()
gputil.check_and_start_gpdb()
config = GPDBConfig()

class case(GPLibPQTestCase):
    """
          View "pg_catalog.pg_stat_replication"
      Column      |           Type           | Modifiers
------------------+--------------------------+-----------
 procpid          | integer                  |
 usesysid         | oid                      |
 usename          | name                     |
 application_name | text                     |
 client_addr      | inet                     |
 client_port      | integer                  |
 backend_start    | timestamp with time zone |
 state            | text                     |
 sent_location    | text                     |
 write_location   | text                     |
 flush_location   | text                     |
 replay_location  | text                     |
 sync_priority    | integer                  |
 sync_state       | text                     |
    """
    def setUp(self):
        # cleanup
        cmd = Command('gpinitstandby', 'gpinitstandby -ar')
        # don't care the result in case standby is not configured
        cmd.run()

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_unixsocket(self):
        """
        Connect via unit socket to walsender.

        @tags sanity
        """

        with PGconn("") as conn:
            res = conn.execute("SELECT test_connect('')")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)

            res = conn.execute("SELECT * FROM pg_stat_replication")
            tup = res.pyobjects().pop(0)

            # client_addr will be NULL in unix socket case
            self.assertIsNone(tup.client_addr)
            # client_port will be -1 in unix socket case
            self.assertEqual(tup.client_port, -1)
            self.assertIsNotNone(tup.sent_location)

            # catch up to the primary
            for i in walrepl.polling(100, 0.5):
                res = conn.execute("SELECT test_receive()")
                if not res.getpyvalue(0, 0):
                    break

            res = conn.execute("SELECT pg_current_xlog_location()")
            current_xlog_location = res.getvalue(0, 0)

            res = conn.execute("SELECT * FROM pg_stat_replication")
            tup = res.pyobjects().pop(0)

            # sent_location follows current_xlog_location.
            # They don't necessarily match, as there should be some
            # window between write and send.
            self.assertTrue(tup.sent_location <= current_xlog_location)

            # These tests should come after gplibpq test library is ready.
            #self.assertEqual(tup.sent_location, current_xlog_location)
            #self.assertEqual(tup.sent_location, tup.write_location)
            #self.assertEqual(tup.sent_location, tup.flush_location)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_localhost(self):
        """
        Connect via network interface on myself.

        @tags sanity
        """

        with PGconn("") as conn:
            res = conn.execute("SELECT test_connect('host=localhost')")

            res = conn.execute("SELECT * FROM pg_stat_replication")
            tup = res.pyobjects().pop(0)
            # client_addr should appear
            self.assertEqual(tup.client_addr, "127.0.0.1")
            # client_port should not be -1
            self.assertNotEqual(tup.client_port, -1)
