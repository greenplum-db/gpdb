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

import tinctest
from mpp.lib.PSQL import PSQL
from mpp.models import SQLTestCase

from gppylib.commands.base import Command
from gppylib.db import dbconn

from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.lib import DbConn
from mpp.gpdb.tests.storage.walrepl.lib import Standby

import os
import shutil

class StandbyRunMixin(object):
    """
    This class adds procedures to set up standby instance and tear it down.
    All test cases that need to run standby instance before actual tests
    can take advantage of this mixin.  An example is as follows:

        class SQLwithStandby(StandbyRunMixin, SQLTestCase):
            def _run_test(self, sql_file, ans_file):
                ...

    Note TestCase class should be more right than this mixin, otherwise
    the TestCase class overrides setUp/tearDown of this mixin.
    """

    basepath = 'base'
    standby_datadir = os.path.abspath(os.path.join(basepath, 'pg_system'))
    standby_port = '5433'
    db_name = 'walrepl'
    fsprefix = os.path.abspath(basepath)

    def createdb(self, dbname):
        need = False
        with DbConn() as conn:
            res = conn.execute("SELECT * FROM pg_database "
                               "WHERE datname = '{0}'".format(dbname))
            if len(res) == 0:
                need = True

        # Do it outside of the block above, as it's accessing template1.
        if need:
            PSQL.run_sql_command('CREATE DATABASE "{0}"'.format(dbname))

    def setUp(self):
        # Remove standby if present.
        # Though the initial intention of tests was to verify
        # without depending on management utility scripts,
        # the reality after all is some other tests might have
        # left a standby and there is not good way other than
        # the gp management script to remove it.
        cmd_str = 'gpinitstandby -a -r'
        cmd = Command(name='gpinitstandby -r', cmdStr=cmd_str)
        tinctest.logger.info(cmd_str)
        cmd.run(validateAfter=False)

        # For each test case we create a fresh standby and start it.
        self.db_name = self.__class__.db_name
        self.standby = Standby(self.standby_datadir,
                                        self.standby_port)

        self.standby.stop()
        shutil.rmtree(self.basepath, True)
        try:
            os.makedirs(self.basepath)
        except OSError, e:
            if e.errno != 17:
                raise
            pass
        self.createdb(self.db_name)

    def tearDown(self):
        # stop standby, but not delete directory, as sometimes it is
        # useful to see what's going wrong.
        self.standby.stop()

        # Clean up filespaces to be tidy.  Although we want to preserve
        # it for the investigation in case of test failure, without cleaning
        # it up it will prevent next tests from running correctly.
        if 'cleanup_filespace' in self._metadata:
            walrepl.cleanupFilespaces(dbname=self.db_name)

    def count_walsender(self):
        """Returns number of active walsender from pg_stat_replication.
        """

        sql = "SELECT count(*) FROM pg_stat_replication"
        with dbconn.connect(dbconn.DbURL(), utility=True) as conn:
            curs = dbconn.execSQL(conn, sql)
            results = curs.fetchall()

        return results[0][0]

    def wait_for_walsender(self, nretry=30, interval=0.5):
        """
        Wait for walsender to be active for a while, and returns when it
        is.  Finally the wait loop ends, in which case it returns 0.
        """

        num_walsender = 0
        for i in walrepl.polling(nretry, interval):
            num_walsender = self.count_walsender()
            if num_walsender > 0:
                break
        return num_walsender

