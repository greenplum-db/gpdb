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
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command
from mpp.models import SQLTestCase

from gppylib.db import dbconn
from gppylib.gparray import GpArray

from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *
from mpp.gpdb.tests.storage.walrepl.lib import NewEnv
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.lib import PgControlData
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.lib import PreprocessFileMixin
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby

import os
import re
import shutil
import socket
import subprocess

pgutil = GpUtility()
pgutil.check_and_start_gpdb()
config = GPDBConfig()

class PromoteTestCase(StandbyRunMixin, PreprocessFileMixin,
                      SQLTestCase):
    sql_dir = 'sql'
    ans_dir = 'ans'
    out_dir = 'output'

    # to be overridden
    promote_using_pg_ctl = True

    def tearDown(self):
        # Clean up filespaces to be tidy.  Although we want to preserve
        # it for the investigation in case of test failure, without cleaning
        # it up it will prevent next tests from running correctly.
        walrepl.cleanupFilespaces(dbname=self.db_name) 
        # remove standby
        cmd = Command('gpinitstandby', 'gpinitstandby -ar')
        cmd.run()

    def fetch_tli(self, data_dir_path):
        controldata = PgControlData(data_dir_path)
        return controldata.get("Latest checkpoint's TimeLineID")
  
    @unittest.skipIf(config.is_multinode(), "Test applies only to a single node cluster")
    def run_test(self):
        """
        Override of SQLTestCase.  Create a base backup and start standby,
        run some SQL in primary side then promote, check if the data is
        streamed correctly.
        """
        sql_file = self.sql_file
        ans_file = self.ans_file
        
        Command('gpinitstandby -r', 'gpinitstandby -ra').run()
        self.assertEqual(self.standby.create(), 0)
        gpact_stdby = GpactivateStandby()
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        # setup script is run on primary while standby is running.
        # .in file will be substitute with runtime information, if any.
        setup_file = sql_file.replace('.sql', '_setup.sql')
        if os.path.exists(setup_file + '.in'):
            self.preprocess_file(setup_file + '.in')
        self.assertTrue(PSQL.run_sql_file(setup_file, dbname=self.db_name))

        if self.promote_using_pg_ctl:
            self.assertTrue(self.standby.promote())
        else:
            self.assertTrue(self.standby.promote_manual())

        # fetch timelineids for both primary and standby (post-promote)
        primary_tli = self.fetch_tli(os.environ.get('MASTER_DATA_DIRECTORY'))
        standby_tli = self.fetch_tli(self.standby_datadir)

        logger.info("primary tli = " + primary_tli)
        logger.info("standby tli after promote = " + standby_tli)

        # primary_tli should be less than standby_tli by 1
        self.assertTrue(int(primary_tli) + 1 == int(standby_tli))

        # SQLTestCase doesn't allow to set port.  Use environ to tell it.
        with NewEnv(PGPORT=self.standby_port,MASTER_DATA_DIRECTORY=self.standby_datadir) as env:
            result = super(PromoteTestCase, self).run_test()
            return result
        # always fail back to old master after test complete
        gpact_stdby.failback_to_original_master()
