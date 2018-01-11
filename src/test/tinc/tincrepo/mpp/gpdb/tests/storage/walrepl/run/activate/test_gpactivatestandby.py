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

import unittest2 as unittest
from tinctest import logger
from mpp.lib.PSQL import PSQL
from mpp.models import SQLTestCase

from gppylib.commands.base import Command
from gppylib.commands import gp
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.lib.config import GPDBConfig

import mpp.gpdb.tests.storage.walrepl.run
import mpp.gpdb.tests.storage.walrepl.lib

import os
import shutil
import signal
import socket

gputil = GpUtility()
config = GPDBConfig()
origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY') 
class gpactivatestandby(mpp.gpdb.tests.storage.walrepl.run.StandbyRunMixin, mpp.gpdb.tests.storage.walrepl.lib.PreprocessFileMixin,
                        SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'sql'
    ans_dir = 'ans'
    out_dir = 'output'

    standby_dir = ''
    standby_host = ''
    standby_port = ''


    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def setUp(self):
        # For each test case we create a fresh standby and start it.
        self.db_name = self.__class__.db_name
        self.createdb(self.db_name)
        gputil.remove_standby()
        gputil.install_standby(new_stdby_host=socket.gethostname()) 
        self.gpinit_stdby = GpinitStandby()
        self.activatestdby = GpactivateStandby()

    def tearDown(self):
        # Clean up filespaces to be tidy.  Although we want to preserve
        # it for the investigation in case of test failure, without cleaning
        # it up it will prevent next tests from running correctly.
        gputil.failback_to_original_master(origin_mdd,self.standby_host,self.standby_dir,self.standby_port)
        # remove standby
        cmd = Command('gpinitstandby', 'gpinitstandby -ar')
        cmd.run()

    def get_gp_dbid(self,standby_dd=''):
        # We use gppylib from the installation
        from gppylib.gp_dbid import GpDbidFile
        return GpDbidFile(standby_dd, True).dbid

    def run_test(self):
        """
        Override of SQLTestCase.  Initialize standby, run some sql,
        then activate it, and check if the data is streamed correctly.
        """
        sql_file = self.sql_file
        ans_file = self.ans_file

        nsender = self.wait_for_walsender()
        self.assertEqual(nsender, 1, 'replication has not begun')

        # setup script is run on primary while standby is running.
        # .in file will be substitute with runtime information, if any.
        setup_file = sql_file.replace('.sql', '_setup.sql')
        if os.path.exists(setup_file + '.in'):
            self.preprocess_file(setup_file + '.in')
        self.assertTrue(PSQL.run_sql_file(setup_file, dbname=self.db_name))

        self.standby_dir = self.activatestdby.get_standby_dd()
        self.standby_port = self.activatestdby.get_standby_port()
        self.standby_host = self.gpinit_stdby.get_standbyhost()
        self.activatestdby.activate()

        datadir = os.path.abspath(self.standby_datadir)
        with walrepl.NewEnv(MASTER_DATA_DIRECTORY=self.standby_dir,
                             PGPORT=self.standby_port) as env:
            result = super(gpactivatestandby, self).run_test()
            sql = 'SHOW gp_dbid'
            result = PSQL.run_sql_command(sql, flags='-A -t')
            self.assertEqual(result.strip(), '1')
            self.assertEqual(self.get_gp_dbid(self.standby_dir), 1, 'gp_dbid should show 1')
            if 'cleanup_filespace' in self._metadata:
                mpp.gpdb.tests.storage.walrepl.lib.cleanupFilespaces(dbname=self.db_name)
        return result

# since the SQL scripts are same as promote test cases,
# we just reference them.
mypath = __file__
mydir = os.path.dirname(mypath)
otherdir = os.path.join(mydir, '..', 'promote')

src = os.path.join(otherdir, 'sql')
dst = os.path.join(mydir, 'sql')
if os.path.exists(dst):
    os.remove(dst)
os.symlink(src, dst)

src = os.path.join(otherdir, 'ans')
dst = os.path.join(mydir, 'ans')
if os.path.exists(dst):
    os.remove(dst)
os.symlink(src, dst)
