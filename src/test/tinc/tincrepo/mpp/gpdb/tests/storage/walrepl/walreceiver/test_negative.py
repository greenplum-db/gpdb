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
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin

import mpp.gpdb.tests.storage.walrepl.lib

import os
import re
import subprocess
import socket
import time
import shutil
import sys
import signal

class neg_test(StandbyRunMixin, MPPTestCase):

    def test_fail_back(self):
 
        """
        This test verifies that the fail-back mode is not allowed.
        Fail-back means original master acting as the new standby.
        """

        # Verify if the database is up. Run some sql.
        PSQL.run_sql_command('DROP table if exists foo')
        Command('remove standby','gpinitstandby -ra').run()
        self.assertEqual(self.standby.create(), 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # Wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        logger.info('Activated WAL Receiver...')

        # Promote the standby & shutdown the old Master
        # Generate a recovery.conf file for the old Master so 
        # to make him the new standby that connects to the new
        # master (originally standby)

        logger.info('Promoting the standby...')
        self.standby.promote()

        dburl = dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        numcontent = gparray.getNumSegmentContents()
        orig_master = gparray.master

        self.standby.remove_catalog_standby(dburl)
 
        if (os.path.exists(os.path.join(orig_master.datadir ,'wal_rcv.pid'))):
            os.remove(os.path.join(orig_master.datadir ,'wal_rcv.pid'))

        logger.info('Stop the original master...')
        cmd = Command("gpstop", "gpstop -aim")
        cmd.run()
        self.assertEqual(cmd.get_results().rc, 0, str(cmd))

        logger.info('Generate recovery.conf for original master to make a new standby...')
        master_recv_conf = open(os.path.join(orig_master.datadir, 'recovery.conf'),'w')
        standby_recv_done = open(os.path.join(self.standby.datadir, 'recovery.done'))
        for line in standby_recv_done:
            master_recv_conf.write(line.replace("port=" + str(os.environ.get('PGPORT')), "port=" + str(self.standby.port)))

        master_recv_conf.close()
        standby_recv_done.close()

        logger.info('Start the old master again (to act as the new standby)...')
        master = gp.MasterStart("Starting orig Master in standby mode",
                                orig_master.datadir, orig_master.port, orig_master.dbid,
                                0, numcontent, None, None, None)

        # -w option would wait forever.
        master.cmdStr = master.cmdStr.replace(' -w', '')
        master.run(validateAfter=True)
        self.assertTrue((master.get_results()).wasSuccessful())

        # Have to do this to give the new standby some time to be active
        subprocess.check_call("psql -c 'create database foo' -p " + str(self.standby.port),
                         shell=True,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
        subprocess.check_call("psql -c 'drop database foo' -p " + str(self.standby.port),
                         shell=True,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)

        time.sleep(3)

        # The new standby can re-start but should not be able to connect to the new
        # master (originally standby). Thats the test
        self.assertTrue(os.path.exists(os.path.join(orig_master.datadir ,'wal_rcv.pid')))
        logger.info('The WAL receiver pid file exists which means the new standby started\n'
              'but still could not connect to the new Master (originally standby) and hence the\n'
              'pid file was not cleared')

        # Remove the recovery.conf file from the new standby directory 
        # as its no more needed
        os.remove(os.path.join(orig_master.datadir,'recovery.conf'))

        logger.info('Stop the original master again...')
        rc = subprocess.Popen('pg_ctl stop -D ' + orig_master.datadir + ' -m immediate',
                              shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        # Perform gpstart to get the original master (& cluster) back again
        cmd = Command("gpstart", "gpstart -a")
        cmd.run()
        self.assertTrue(cmd.get_results().rc in (0, 1), str(cmd))

        logger.info ('Pass')

