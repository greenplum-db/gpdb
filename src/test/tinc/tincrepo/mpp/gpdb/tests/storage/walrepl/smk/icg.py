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

from mpp.gpdb.tests.storage.walrepl.lib import polling
from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby


import os
import subprocess
import socket
import time
import shutil
import sys
"""
    Moving the name of the file to not start with test - This is to avoid discover running the test
"""
class icg(MPPTestCase):
    def count_wal_sender(self):
        sql = "SELECT count(*) FROM pg_stat_replication"
        with dbconn.connect(dbconn.DbURL()) as conn:
            curs = dbconn.execSQL(conn, sql)
            results = curs.fetchall()

        return results[0][0]

    def test_icg(self):

        # 1. Verify if the system is UP and there is no WAL Receiver running
        # 2. Perform basebackup and deploy it into some dest. directory
        # 3. Copy recover.conf into the dest. directory to be used by Standby
        # 4. Initiate the Standby using the Master (primary) postmaster
        #    paramerters
        # 5. Once the WAL receiver waits for the next record to arrive, perform
        #    installCheck-good

        # 0. Stop standby if it's running
        standby = Standby('base', 5433)
        standby.stop()

        # 1. Verify if the system is UP and there is no WAL sender running
        self.assertEqual(self.count_wal_sender(), 0)
        logger.info('No active WAL Receiver found')

        # Set environmental variable GPSRC for now for make installcheck purpose
        source_file = sys.modules[self.__class__.__module__].__file__
        source_dir = os.path.dirname(source_file)
        os.environ['GPSRC'] = os.path.join(source_dir, '../../../../')

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
            num_walsender = self.count_wal_sender()
            if num_walsender > 0:
                break
        self.assertEqual(num_walsender, 1)

        logger.info('Activated WAL Receiver...')

        # 6. Run installcheck-good
        self.assertTrue(os.environ["GPSRC"])

        installCheckGoodPath = os.path.join(os.environ.get('GPSRC'),
                                                           'test',
                                                           'regress')

        self.assertTrue(os.path.exists(installCheckGoodPath))
        os.chdir(installCheckGoodPath)

        regression_diffs = os.path.join(installCheckGoodPath, 'regression.diffs')
        if os.path.exists(regression_diffs):
            os.remove(regression_diffs)

        logger.info('Perform InstallCheck-Good...')
        subprocess.check_call('make installcheck-good', shell=True)

        # Verify installcheck result by checking if regression.diff is present.
        self.assertTrue(not os.path.exists(regression_diffs))
