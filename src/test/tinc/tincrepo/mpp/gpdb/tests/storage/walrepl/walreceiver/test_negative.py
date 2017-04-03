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

    def test_negative(self):
        for trigger_content in ['wait_before_send','wait_before_rcv']:
            for master_shutdown_mode in ['immediate','fast','smart']:
                self.connection_scenario(trigger_content, master_shutdown_mode)
        logger.info('All tests passed!')

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

    def generate_trigger_file(self, filepath, filename, content):
        self.assertTrue(os.path.exists (filepath))
        self.assertTrue(os.path.isdir(filepath))
        self.assertTrue(filename and content)

        trigger_file = open(os.path.join(filepath,filename), "wb")
        trigger_file.write(content)

        trigger_file.close()

    def connection_scenario(self, trigger_content, master_shutdown_mode):

        #  Verify if the system is UP
        #  Setup a standby
        #  Once the WAL receiver starts, signal it to suspend based on where the
        #  input parameter wants
        #  Once suspended, shutdown the Master(primary) based on the input mode.
        #  Release the WAL receiver and it should fail (dead). But later after waiting
        #  for some time it should re-try to connect to the Master and fail again
        #  till the actual Master comes up again.
        #Note :- Sleeps used in this test are a little larger than normal times
        #to cope up with events like for e.g. spawning of WAL Receiver which entirely
        #depends on when the startup process signals the Postmaster to do it

        # Verify if the database is up. Run some sql.
        PSQL.run_sql_command('DROP table if exists foo')
        Command('remove standby', 'gpinitstandby -ra').run()
        self.assertEqual(self.standby.create(), 0)

        # Trigger & evidence files cleanup
        if (os.path.exists(os.path.join(self.standby.datadir,'wal_rcv.pid'))):
            os.remove(os.path.join(self.standby.datadir,'wal_rcv.pid'))

        if (os.path.exists(os.path.join(self.standby.datadir,
                                        'wal_rcv_test'))):
            os.remove(os.path.join(self.standby.datadir, 'wal_rcv_test'))

        # Setup a standby
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # Wait for the walreceiver to start
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)
        logger.info('Activated WAL Receiver...')

        # Cleanup the standby configuration from Master catalog
        # This is to avoid re-start of the standby on Master re-start 
        dburl = dbconn.DbURL()
        self.standby.remove_catalog_standby(dburl)

        #  Once the WAL receiver starts, signal it to suspend based on where the
        #  input parameter wants
        wal_rcv_pid = self.get_pid_having_keyword('wal receiver process')

        logger.info('Suspending WAL Receiver(' + str(wal_rcv_pid) +') ' + 'with ' + trigger_content)

        self.generate_trigger_file(self.standby.datadir, 'wal_rcv_test', trigger_content)
        os.kill(wal_rcv_pid, signal.SIGUSR2)

        time.sleep(10)
        self.assertTrue(not os.path.exists(os.path.join(self.standby.datadir,'wal_rcv.pid')))

        #  Once suspended, shutdown the Master(primary) based on the input mode.
        logger.info('Shutdown the Master in ' + master_shutdown_mode + ' mode')
        if master_shutdown_mode == 'immediate':
            cmd = Command("gpstop master immediate", "gpstop -aim")
        elif master_shutdown_mode == 'smart':
            cmd = Command("gpstop master smart", "gpstop -am")
        elif master_shutdown_mode == 'fast':
            cmd = Command("gpstop master fast", "gpstop -afm")
        cmd.run()
        self.assertEqual(cmd.get_results().rc, 0, str(cmd))

        # Release (resume) the WAL receiver and it should fail (dead). But later after waiting
        # for some time it should re-try to connect to the Master and fail again
        # till the actual Master comes up again.
        logger.info('Resume the WAL Receiver(' + str(wal_rcv_pid) + ')')
        self.generate_trigger_file(self.standby.datadir, 'wal_rcv_test', "resume")
        os.kill(wal_rcv_pid, signal.SIGUSR2)
        time.sleep(10)

        # The pid file should exist. This is a proof that the WAL receiver came up
        # but did not get a chance to connect to the Master and hence did not clean up
        # the pid file
        self.assertTrue(os.path.exists(os.path.join(self.standby.datadir,'wal_rcv.pid')))

        logger.info('The WAL receiver pid file exists which means it restarted\n'
              'but still could not connect to the Master (primary) and hence the\n'
              'pid file was not cleared')

        # Stop the standby as its of no use anymore
        rc = subprocess.Popen('pg_ctl stop -D ' + self.standby.datadir + ' -m immediate',
                              shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        #TODO RKT - Ideally, only the Primary should have been started here. But given the current nature of
        #gpstart supporting Master start only in utility mode and WAL repl not supporting utility
        #connection to the Master, normal gpstart (Master, Standby and Segment restart) will be used
        #for time being. This will be changed once utility support is added to WAL based repl.
        cmd = Command("gpstart", "gpstart -a")
        cmd.run()
        self.assertTrue(cmd.get_results().rc in (0, 1), str(cmd))

        logger.info('Pass (' + trigger_content + ',' + master_shutdown_mode + ')')

        # Cleanup for the next iteration
        shutil.rmtree(self.standby.datadir, True)


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

