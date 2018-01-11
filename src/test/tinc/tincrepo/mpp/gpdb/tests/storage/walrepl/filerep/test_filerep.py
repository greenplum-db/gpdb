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
from time import sleep
import unittest2 as unittest

import tinctest
from gppylib.commands.base import Command
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby

pgutil = GpUtility()
pgutil.check_and_start_gpdb()
origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY')
class FilerepTestCase(MPPTestCase):
    '''
     Test purpose: put the cluster into different transition
     status, run gprecoverseg from change tracking, resync
     transition, check if cluster can be recovered
    '''

    standby_host = ''
    standby_mdd = ''
    standby_port = ''

    def __init__(self,methodName):
        self.gpact_stdby = GpactivateStandby()
        super(FilerepTestCase,self).__init__(methodName)


    def setUp(self):
        # We should forcibly recreate standby, as it might has been promoted.
        pgutil.remove_standby()
        pgutil.install_standby()

    def tearDown(self):
        pgutil.failback_to_original_master(origin_mdd,self.standby_host,self.standby_mdd,self.standby_port)


    def wait_till_insync_transition(self, gpact_standby, host, port, mdd, keyword='None', expected=False):
        """
        PURPOSE:
            Poll till insync state achieved: Wait till all segments transition to insync state
        """

        cmd_sync = 'gpstate -e'
        delay = 0
        loop = True
        while loop and delay < 20:
            (rc,stdout) = gpact_standby.run_remote(host,cmd_sync,port,mdd) 
            tinctest.logger.info('result from gpstate -e \n  %s'%stdout)
            if expected and stdout.find(keyword) != -1:
                break
            if not expected and stdout.find(keyword) == -1:
                break
            sleep(60) # Wait for a minute before poling again 
            delay += 1
        self.assertNotEqual(delay,5)

    def wait_till_changetracking_transition(self, host, port):

        changetracking_sql = """ select count(*) from gp_segment_configuration
        where mode = 'c' and status = 'u'"""

        tinctest.logger.info('waiting for segments to come into changetracking')
        mins_elapsed = 0
        while mins_elapsed < 20:
            ct_count = None
            try:
                ct_count = pgutil.run_SQLQuery(changetracking_sql, 'template1', host, port)
            except Exception as e:
                tinctest.logger.debug('encountered error while trying to connect to database (%s)' % str(e))

            if ct_count and int(ct_count[0][0]) > 0:
                break
            mins_elapsed += 1
            sleep(60)

    def test_failover_in_change_track(self):
        """
        bring down mirror segments, failover to standby, run gprecoverseg.
        """
        tinctest.logger.info("-failover to standby in change tracking  and recoverseg")
        # get standby host, port, and master data directory
        activatestdby = GpactivateStandby()
        self.standby_host = activatestdby.get_current_standby()
        self.standby_mdd = activatestdby.get_standby_dd()
        self.standby_port = activatestdby.get_standby_port()
        # bring down mirror segments
        Command('fault inject mirror segment', 'gpfaultinjector  -f filerep_consumer  -m async -y fault -r mirror -H ALL').run()
        activatestdby.activate()        

        # wait till segments come up in change tracking
        self.wait_till_changetracking_transition(self.standby_host, self.standby_port)

        (rc, stdout) = activatestdby.run_remote(self.standby_host,'gprecoverseg -a',self.standby_port,self.standby_mdd)
        tinctest.logger.info("in test_failover_in_change_track:  gprecoverseg -a:  %s"%stdout)

        # check if all segments are up and sync
        keyword = 'All segments are running normally' 
        self.wait_till_insync_transition(activatestdby,self.standby_host,self.standby_port,self.standby_mdd,keyword,True)


    def test_failover_insync(self):
        """ 
        bring down mirror segments,suspend in resync mode,failover to standby, run gprecoverseg.
        """
        tinctest.logger.info("-failover to standby in resync and recoverseg")
        # get standby host, port, and master data directory
        activatestdby = GpactivateStandby()
        self.standby_host = activatestdby.get_current_standby()
        self.standby_mdd = activatestdby.get_standby_dd()
        self.standby_port = activatestdby.get_standby_port()

        # bring down mirror segments and suspend
        Command('fault inject mirror segment', 'gpfaultinjector  -f filerep_consumer  -m async -y fault -r mirror -H ALL').run()

        # wait till segments come up in change tracking
        self.wait_till_changetracking_transition('localhost', os.environ['PGPORT'])

        Command('Injecting fault to suspend resync','gpfaultinjector -f filerep_resync -m async -y suspend -r primary -H ALL').run()
        Command('recover and suspend in insync','gprecoverseg -a').run()

        activatestdby.activate()   

        # Injecting Fault  to resume resync
        resume_resync_cmd = 'gpfaultinjector -f filerep_resync -m async -y resume -r primary -H ALL'
        activatestdby.run_remote(self.standby_host, resume_resync_cmd, self.standby_port, self.standby_mdd) 
        # Injecting Fault  to reset resync
        reset_resync_cmd = 'gpfaultinjector -f filerep_resync -m async -y reset -r primary -H ALL'
        activatestdby.run_remote(self.standby_host, reset_resync_cmd, self.standby_port, self.standby_mdd)

        # check if all segments are up and sync
        keyword = 'All segments are running normally'       
        self.wait_till_insync_transition(activatestdby,self.standby_host,self.standby_port,self.standby_mdd,keyword,True)        
       

    def test_inject_mirror_after_promote(self):
        """ 
        Promote to standby, bring down mirror segments,run gprecoverseg.
        """
        tinctest.logger.info("-failover to standby, inject mirror segments, and recoverseg")
        # get standby host, port, and master data directory
        activatestdby = GpactivateStandby()
        self.standby_host = activatestdby.get_current_standby()
        self.standby_mdd = activatestdby.get_standby_dd()
        self.standby_port = activatestdby.get_standby_port()
        activatestdby.activate()   

        # inject the mirror segments from new master
        inject_cmd = 'gpfaultinjector  -f filerep_consumer  -m async -y fault -r mirror -H ALL'
        activatestdby.run_remote(self.standby_host, inject_cmd, self.standby_port, self.standby_mdd) 

        # wait till segments come up in change tracking
        self.wait_till_changetracking_transition(self.standby_host, self.standby_port)

        # recoverseg from new master
        (rc, stdout) = activatestdby.run_remote(self.standby_host,'gprecoverseg -a',self.standby_port,self.standby_mdd)
        tinctest.logger.info("in test_inject_mirror_after_promote:  gprecoverseg -a:  %s"%stdout) 

        # check if all segments are up and sync
        keyword = 'All segments are running normally'
        self.wait_till_insync_transition(activatestdby,self.standby_host,self.standby_port,self.standby_mdd,keyword, True)      


    def test_inject_primary_after_promote(self):
        """ 
        Promote to standby, bring down primary segments, run gprecoverseg.
        """
        tinctest.logger.info("-failover to standby, inject primary segments, and recoverseg")
        # get standby host, port, and master data directory
        activatestdby = GpactivateStandby()
        self.standby_host = activatestdby.get_current_standby()
        self.standby_mdd = activatestdby.get_standby_dd()
        self.standby_port = activatestdby.get_standby_port()
        activatestdby.activate()   

        # bring down primary segments
        inject_cmd = 'gpfaultinjector -f postmaster -m async -y panic -r primary -H ALL'
        activatestdby.run_remote(self.standby_host, inject_cmd, self.standby_port, self.standby_mdd)

        # wait till segments come up in change tracking
        self.wait_till_changetracking_transition(self.standby_host, self.standby_port)

        # recoverseg from new master
        (rc, stdout) = activatestdby.run_remote(self.standby_host,'gprecoverseg -a',self.standby_port,self.standby_mdd)
        tinctest.logger.info("in test_inject_primary_after_promote:  gprecoverseg -a:  %s"%stdout)

        keyword = 'Segment Pairs in Resynchronization'
        self.wait_till_insync_transition(activatestdby,self.standby_host,self.standby_port,self.standby_mdd,keyword,False)

        # rebalance from new master
        (rc, stdout) = activatestdby.run_remote(self.standby_host,'gprecoverseg -ra',self.standby_port,self.standby_mdd)
        tinctest.logger.info("in test_inject_primary_after_promote:  gprecoverseg -ar:  %s"%stdout)

        # check if all segments are up and sync
        keyword = 'All segments are running normally'
        self.wait_till_insync_transition(activatestdby,self.standby_host,self.standby_port,self.standby_mdd, keyword, True)        
