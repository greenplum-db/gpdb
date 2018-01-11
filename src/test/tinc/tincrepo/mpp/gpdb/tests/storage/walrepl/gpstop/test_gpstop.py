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

import os
import socket
from time import sleep, time
import unittest2 as unittest
import subprocess

import tinctest
from tinctest.lib import local_path
from gppylib.commands.base import Command, REMOTE
from mpp.models import MPPTestCase
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

class GpstopTestCase(MPPTestCase):
   '''testcase for gpstart''' 
    
   origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY') 
   
   def __init__(self,methodName):
       self.gputil = GpUtility()
       super(GpstopTestCase,self).__init__(methodName)
    
    
   def setUp(self):
       self.gputil.check_and_start_gpdb()     
       stdby_presence = self.gputil.check_standby_presence()
       if  stdby_presence:
           self.gputil.remove_standby()
       self.gputil.install_standby()

   def tearDown(self):
       self.gputil.remove_standby()
       self.gputil.run('gpstart -a')
       self.gputil.run('gprecoverseg -a')

   def test_gpstop_from_master(self):
       self.assertTrue(self.gputil.gpstop_and_verify())
       self.gputil.run('gpstart -a')

   def test_gpstop_master_only(self):
       self.assertTrue(self.gputil.gpstop_and_verify(option = '-m'))
       self.gputil.run('gpstart -a')

   def test_gpstop_fast(self):
       #run transactions, and stop fast, check if transaction aborted, and the cluster was stopped
       self.assertTrue(self.gputil.gpstop_and_verify(option = '-M fast'))
       self.gputil.run('gpstart -a')

   def test_gpstop_immediate(self):
       self.assertTrue(self.gputil.gpstop_and_verify(option = '-M immediate'))
       self.gputil.run('gpstart -a')

   def test_gpstop_smart(self):
       self.assertTrue(self.gputil.gpstop_and_verify(option = '-M smart'))
       self.gputil.run('gpstart -a')
  
   def test_gpdb_restart(self):
       self.assertTrue(self.gputil.gpstop_and_verify('-r'))

   def test_gpdb_reload(self):
       self.assertTrue(self.gputil.gpstop_and_verify('-u'))

   def test_gpstop_except_stdby(self):
       self.assertTrue(self.gputil.gpstop_and_verify('-y'))
       self.gputil.run('gpstart -y')

   def test_gpstop_after_failover(self):
       tinctest.logger.info("test gpstop from new master after failover")
       activatestdby = GpactivateStandby()
       standby_host = activatestdby.get_current_standby()
       standby_port =  activatestdby.get_standby_port()
       standby_mdd = activatestdby.get_standby_dd()
       activatestdby.activate()    
       (rc,stdout)=activatestdby.run_remote(standby_host, rmt_cmd='gpstop -a -M fast',
                                                 pgport=standby_port,standbydd=standby_mdd) 
       self.assertEqual(0,rc)
       activatestdby.run_remote(standby_host, rmt_cmd='gpstart -a',
                                                 pgport=standby_port,
                                                 standbydd=standby_mdd)
       self.gputil.failback_to_original_master(self.origin_mdd, standby_host, standby_mdd, standby_port) 

"""
   def test_gpstop_progress_bar(self):
       tinctest.logger.info('test the progress bar for gpstop')
       (rc, stdout) = self.gputil.run('gpstop -a')
       self.assertIn('100.00%', stdout)

   def test_gpstop_kill_9_process(self):
       tinctest.logger.info('test kill -9 of the leftover postgres processes')
       conf = GPDBConfig()
       (hostname, port) = conf.get_hostandport_of_segment(1)
       tinctest.logger.info('port = %s' % port)
       sleep(10)
       self.gputil.stop_logger_process(hostname, port)
       self.assertTrue(self.gputil.gpstop_and_verify())

   def test_gpstop_kill_9_process_for_master(self):
       tinctest.logger.info('test kill -9 of the leftover postgres processes')
       conf = GPDBConfig()
       (hostname, port) = conf.get_hostandport_of_segment(-1)
       tinctest.logger.info('port = %s' % port)
       sleep(10)
       self.gputil.stop_logger_process(hostname, port)
       self.assertTrue(self.gputil.gpstop_and_verify())

   def test_gpstop_timeout(self):
       tinctest.logger.info('test the timeout value for postgres process')
       conf = GPDBConfig()
       (hostname, port) = conf.get_hostandport_of_segment(0)
       tinctest.logger.info('port = %s' % port)
       sleep(10)
       self.gputil.stop_postmaster_process(hostname, port)
       (hostname, port) = conf.get_hostandport_of_segment(1)
       tinctest.logger.info('port = %s' % port)
       self.gputil.stop_postmaster_process(hostname, port)
       while conf.is_not_insync_segments():
           tinctest.logger.info('Waiting for DB to go into CT mode')
           sleep(1)
       tinctest.logger.info('attempting gpstop on the databse')
       sleep(10)
       start = time()
       self.gputil.gpstop_and_verify('-M fast')
       end = time()
       tinctest.logger.info('Total time for stop = %s' % (end - start))
       self.assertTrue(end - start < 6 * 60)

   def test_gpstop_shared_memory_and_semaphores(self):
       tinctest.logger.info('test the cleanup of semaphores and shared memory')
       conf = GPDBConfig()
       (hostname, port) = conf.get_hostandport_of_segment(psegmentNumber=0, pRole='p')
       cmd = Command('kill -9 the segment', cmdStr="ps ux | grep '[p]ostgres \-D.*%s' | awk '{print \$2}' | xargs kill -9" % port, ctxt=REMOTE, remoteHost=hostname)
       cmd.run(validateAfter=True)
       sleep(10)
       while conf.is_not_insync_segments():
           tinctest.logger.info('Waiting for Db to go into CT mode')
           sleep(1)
       (rc, stdout) = self.gputil.run('psql -d template1 -c "select * from gp_segment_configuration"')
       (rc, stdout) = self.gputil.run('gpstop -a -M fast')
       self.assertEqual(rc, 0)
       (rc, stdout) = self.gputil.run_remote(hostname, 'ipcs -a | grep ^[0-9]')
       output = stdout.strip().split('\n')
       self.assertTrue(len(output) == 1)
       self.assertIn('ipcs -a', output[0])

   def test_gpstop_hung_gpmmon(self):
       tinctest.logger.info('test the termination of hung gpmmon process')
       conf = GPDBConfig()
       (hostname, port) = conf.get_hostandport_of_segment(-1)
       hosts = conf.get_hosts()
       (rc, stdout) = self.gputil.run('gpperfmon_install --enable --password p@$$word --port %s' % port)
       self.assertEqual(rc, 0)
       (rc, stdout) = self.gputil.run('gpstop -ar')
       self.assertEqual(rc, 0)
       self.gputil.stop_gpmmon_process(hostname)
       self.assertTrue(self.gputil.gpstop_and_verify('-M fast'))
       self.gputil.verify_gpmmon()
       for host in hosts:
           self.gputil.verify_gpsmon(host)

   def test_gpstop_hung_gpsmon(self):
       tinctest.logger.info('test the termination of hung gpsmon process')
       conf = GPDBConfig()
       (hostname, port) = conf.get_hostandport_of_segment(-1)
       (rc, stdout) = self.gputil.run('gpperfmon_install --enable --password p@$$word --port %s' % port)
       self.assertEqual(rc, 0)
       (rc, stdout) = self.gputil.run('gpstop -ar')
       self.assertEqual(rc, 0)
       hosts = conf.get_hosts()
       for host in hosts:
           self.gputil.stop_gpsmon_process(hostname)
       self.assertTrue(self.gputil.gpstop_and_verify('-M fast'))
       self.gputil.verify_gpmmon()
       for host in hosts:
           self.gputil.verify_gpsmon(host)
"""
