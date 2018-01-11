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

from mpp.models import MPPTestCase
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

import subprocess
import os

gputil = GpUtility()
gputil.check_and_start_gpdb()
config = GPDBConfig()

class GPLibPQTestCase(MPPTestCase):
    """This test case is to use gplibpq library, which is SQL function
    interface for walreceiver's libpq part.
                       List of functions
 Schema |      Name       | Result data type | Argument data types
--------+-----------------+------------------+----------------------
 public | test_connect    | boolean          | text
 public | test_disconnect | boolean          |
 public | test_receive    | boolean          |
 public | test_scenario1  | boolean          | text
 public | test_send       | boolean          |
    """

    @classmethod
    def setUpClass(cls):
        """
        Install gplibpq module.
        """
        if not config.is_multinode():        
            # Build it before testing.
            thisdir = os.path.dirname(__file__)
            builddir = os.path.join(thisdir, 'gplibpq')
            subprocess.check_call(['make', '-C', builddir, 'install'])

            # execute CREATE FUNCTION statements
            install_sql = os.path.join(builddir, 'install.sql')
            subprocess.check_call(['psql', '-f', install_sql])
        
        else:
            pass

    def tearDown(self):
        # each test should wait for the wal sender to disappear
        for i in walrepl.polling(30, 0.5):
            with PGconn("") as conn:
                cnt = self.count_walsender(conn)
                if cnt <= 0:
                    break

    def count_walsender(self, conn):
        res = conn.execute("""SELECT count(*) FROM pg_stat_replication""")

        if res.status() != PGRES_TUPLES_OK:
            return -1
        return res.getpyvalue(0, 0)

