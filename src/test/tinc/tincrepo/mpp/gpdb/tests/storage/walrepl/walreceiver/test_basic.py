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

import unittest2 as unittest
from tinctest import logger

from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.walreceiver import GPLibPQTestCase
from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.lib.config import GPDBConfig

import select

gputil = GpUtility()
gputil.check_and_start_gpdb()
config = GPDBConfig()

class case(GPLibPQTestCase):
    """Basic test cases to see gplibpq and walsender are talking correctly."""

    def setUp(self):
        # cleanup
        cmd = Command('gpinitstandby', 'gpinitstandby -ar')
        # don't care the result in case standby is not configured
        cmd.run()
    
    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_connect(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            res = conn.execute("SELECT test_connect('')")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)
            self.assertTrue(res.getvalue(0, 0, convert=True))

            cnt = self.count_walsender(conn)
            self.assertEqual(cnt, 1)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_connect_fail(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            # the connection failure when conninfo is wrong.
            res = conn.execute("SELECT test_connect('host=none')")
            self.assertEqual(res.status(), PGRES_FATAL_ERROR)

            errmsg = res.error_message()
            self.assertIn("could not connect", errmsg)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_connect_host(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            res = conn.execute("SELECT test_connect('host=localhost')")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)

            cnt = self.count_walsender(conn)
            self.assertEqual(cnt, 1)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_disconnect(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            # check disconnect does not fail even before connect
            res = conn.execute("SELECT test_disconnect()")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)
            self.assertTrue(res.getvalue(0, 0, convert=True))

            # check if disconnect clears the walsender
            res = conn.execute("SELECT test_connect('')")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)
            res = conn.execute("SELECT test_disconnect()")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)

            # wait for the backend terminates...
            for i in walrepl.polling(10, 0.5):
                cnt = self.count_walsender(conn)
                if cnt == 0:
                    break

            self.assertEqual(cnt, 0)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_receive_fail(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            # Simply fails if not connected.
            res = conn.execute("SELECT test_receive()")
            self.assertEqual(res.status(), PGRES_FATAL_ERROR)
            errmsg = res.error_message()
            self.assertIn("connection pointer is NULL", errmsg)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_receive(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            res = conn.execute("SELECT test_connect('')")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)

            # the first call should get reply.
            res = conn.execute("SELECT test_receive()")
            # the second call should time out.
            res = conn.execute("SELECT test_receive()")
            res = conn.execute("SELECT test_receive()")
            res = conn.execute("SELECT test_receive()")
            res = conn.execute("SELECT test_receive()")
            res = conn.execute("SELECT test_receive()")
            res = conn.execute("SELECT test_receive()")
            self.assertFalse(res.getvalue(0, 0, convert=True))

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_send_fail(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            # Simply fails if not connected.
            res = conn.execute("SELECT test_send()")
            self.assertEqual(res.status(), PGRES_FATAL_ERROR)
            errmsg = res.error_message()
            self.assertIn("connection pointer is NULL", errmsg)

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_send(self):
        """
        @tags sanity
        """

        with PGconn("") as conn:
            res = conn.execute("SELECT test_connect('')")
            self.assertEqual(res.status(), PGRES_TUPLES_OK)

            res = conn.execute("SELECT test_send()")
            self.assertTrue(res.getvalue(0, 0, convert=True))

if __name__ == '__main__':
    # just a test of async interface.
    def printer(arg, res):
        res = PGresult(res)
        print res.error_message()
    with PGconn("") as conn:
        handler = NoticeReceiverFunc(printer)
        conn.set_notice_receiver(handler, None)
        while True:
            conn.send_query("SELECT test_scenario1('')")
            fd = [conn]
            ready = select.select(fd, [], [], 0.1)[0]
            if len(ready) > 0:
                if conn.consume_input() == 0:
                    raise StandardError("cannot read input")
                if conn.is_busy() == 0:
                    res = conn.get_result()
