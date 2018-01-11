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
from time import sleep
import unittest2 as unittest

import tinctest
from gppylib.commands.base import Command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby


class GpstartTestCase(MPPTestCase):
    '''
    testcase for gpstart

    gpstart may return status code 1 as well as 0 in the success case.  The
    difference is whether it produces WARNING or not, but here we don't care.
    '''
    origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY')

    def __init__(self,methodName):
        self.gputil = GpUtility()
        self.stdby = StandbyVerify()
        super(GpstartTestCase,self).__init__(methodName)

    def setUp(self):
        self.gputil.check_and_start_gpdb()
        stdby_presence = self.gputil.check_standby_presence()
        # We should forcibly recreate standby, as it might has been promoted.
        if stdby_presence:
            self.gputil.remove_standby()
        self.gputil.install_standby()

    def tearDown(self):
        self.gputil.remove_standby()
        
    """
    Gpstart test cases in recovery mode
    """

    def test_gpstart_from_master(self):
        """
        tag
        """
        self.gputil.check_and_stop_gpdb()
        (rc, stdout) = self.gputil.run('gpstart -a ')
        self.assertIn(rc, (0, 1))
        self.assertTrue(self.gputil.gpstart_and_verify())
        sleep(2)
        self.assertTrue(self.stdby.check_gp_segment_config(),'standby master not cofigured')
        self.assertTrue(self.stdby.check_pg_stat_replication(),'standby not in replication status')
        self.assertTrue(self.stdby.check_standby_processes(), 'standby processes not running')
        (rc, output) = self.gputil.run(command = 'ps -ef|grep "wal sender "|grep -v grep')
        self.assertIsNotNone(output)

    def test_gpstart_master_only(self):
        """
        tag
        """
        self.gputil.check_and_stop_gpdb()
        (rc, stdout) = self.gputil.run('export GPSTART_INTERNAL_MASTER_ONLY=1; '
                                   'gpstart -a -m ')
        self.assertIn(rc, (0, 1))
        self.assertTrue(self.gputil.gpstart_and_verify())
        (rc,output) = self.gputil.run('PGDATABASE=template1 '
                                  "PGOPTIONS='-c gp_session_role=utility' "
                                  'psql')
        self.assertEqual(rc, 0)
        (rc, output) = self.gputil.run('psql template1')
        # should fail due to master only mode
        self.assertEqual(rc, 2)
        self.gputil.run('gpstop -a -m')
        self.gputil.run('gpstart -a')


    def test_gpstart_restricted_mode_master(self):
        """Test -R option with standby."""

        self.gputil.check_and_stop_gpdb()
        (rc, stdout) = self.gputil.run('gpstart -a -R')
        self.assertIn(rc, (0, 1))
        self.assertTrue(self.gputil.gpstart_and_verify())
        (rc,output) = self.gputil.run(command = 'psql template1')
        self.assertIn(rc, (0, 1))
        self.gputil.run('gpstop -ar')


    def test_gpstart_master_w_timeout(self):
        """Test -t option with standby."""

        self.gputil.check_and_stop_gpdb()
        (rc, output) = self.gputil.run('gpstart -a -t 30')
        self.assertIn(rc, (0, 1))
        self.assertTrue(self.gputil.gpstart_and_verify())
        self.gputil.run('gpstop -ar')

    def test_gpstart_no_standby(self):
        """Test -y with standby configured."""

        self.gputil.check_and_stop_gpdb()
        (rc, stdout) = self.gputil.run('gpstart -a -y')
        self.assertIn(rc, (0, 1))
        self.assertTrue(self.gputil.gpstart_and_verify())
        self.assertFalse(self.stdby.check_standby_processes(),
                         'gpstart without standby failed, standby was running')
        self.gputil.run('gpstop -ar')

    def test_gpstart_wo_standby(self):
        """Test -y without standby configured."""

        self.gputil.remove_standby()
        self.gputil.check_and_stop_gpdb()
        (rc, stdout) = self.gputil.run('gpstart -a -y')
        self.assertIn(rc, (0, 1))
        self.assertTrue(self.gputil.gpstart_and_verify())
        self.assertFalse(self.stdby.check_standby_processes(), 'standby processes presented')
        self.gputil.run('gpstop -ar')

    """
    Gpstart, test case in failover mode
    """

    def test_gpstart_master_only_after_failover(self):
        """
        for test purpose, failing back to old master should
              remove standby from primary after activate standby
        """
        tinctest.logger.info("start master only with -m option after failover")
        activatestdby = GpactivateStandby()
        standby_host = activatestdby.get_current_standby()
        standby_mdd = activatestdby.get_standby_dd()
        standby_port = activatestdby.get_standby_port()
        activatestdby.activate()
        self.stdby._run_remote_command(standby_host,command = 'gpstop -a')
        stdout = self.stdby._run_remote_command(standby_host,command = 'export  GPSTART_INTERNAL_MASTER_ONLY=1; gpstart -a -m')
        self.assertNotRegexpMatches(stdout,"ERROR","Start master only after failover failed")
        self.assertTrue(self.gputil.gpstart_and_verify(master_dd = standby_mdd, host = standby_host))
        self.stdby._run_remote_command(standby_host,command = 'gpstop -a -m')
        self.gputil.run(command = 'gpstop -ar')
        self.gputil.failback_to_original_master(self.origin_mdd, standby_host, standby_mdd, standby_port)

    def test_gpstart_master_after_failover(self):
        """
        failover, start from new master, then recover the cluster back to
        have the old master active.
        """
        tinctest.logger.info("failover, and run gpstart master test")
        self.gputil.check_and_start_gpdb()
        activatestdby = GpactivateStandby()
        standby_host = activatestdby.get_current_standby()
        standby_mdd = activatestdby.get_standby_dd()
        standby_port = activatestdby.get_standby_port()
        activatestdby.activate()
        self.stdby._run_remote_command(standby_host, command = 'gpstop -a')
        stdout = self.stdby._run_remote_command(standby_host,command = 'gpstart -a')
        self.assertNotRegexpMatches(stdout,"FATAL","ERROR")
        self.assertTrue(self.gputil.gpstart_and_verify(master_dd = standby_mdd, host = standby_host))
        self.gputil.failback_to_original_master(self.origin_mdd, standby_host, standby_mdd, standby_port)

    def test_gpstart_original_master_after_promote(self):
        """
        failover, start from new master, then recover the cluster back to
        have the old master active.
        """
        tinctest.logger.info("activate and run gpstart for original master")
        activatestdby = GpactivateStandby()
        standby_host = activatestdby.get_current_standby()
        standby_mdd = activatestdby.get_standby_dd()
        standby_port = activatestdby.get_standby_port()
        activatestdby.activate()
        (rc, stdout) = self.gputil.run('gpstart -a -v')
	self.gputil.run('pg_controldata %s' % self.origin_mdd)
	self.stdby._run_remote_command(standby_host, command = 'pg_controldata %s' % standby_mdd)
        self.assertNotEqual(rc, 0)
        # This below error message comes from gpstart product code (if its modified change it here as well.)
        self.assertRegexpMatches(stdout,"Standby activated, this node no more can act as master.")
        self.gputil.failback_to_original_master(self.origin_mdd, standby_host, standby_mdd, standby_port)
