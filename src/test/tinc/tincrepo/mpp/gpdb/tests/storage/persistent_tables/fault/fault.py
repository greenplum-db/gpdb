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

import tinctest

from time import sleep
from datetime import datetime
from random import randint
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from mpp.gpdb.tests.storage.lib.common_utils import checkDBUp
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.config import GPDBConfig
from tinctest import TINCTestCase

'''
Fault Tests for different scenarios of PT:
'''
class FaultInjectionTests(TINCTestCase):
        
    def test_failover_to_mirror_during_transactions(self):
        tinctest.logger.debug('Fault Injection Tests - starting the failover to mirror scenario')
        #Sleep introduced so that the concurrently running generate_sqls gets time 
        #to actually generate some load
        tinctest.logger.info('Sleep introduced of 120 sec')
        sleep(120)
        newfault = Fault()
        newfault.kill_processes_with_role('p')
        newfault.restart_db()
        tinctest.logger.debug('Fault Injection Tests - failover to mirror completed')
        
    def post_master_reset_during_transaction(self,kill_all = True):
        tinctest.logger.debug('Fault Injection Tests - starting postmaster reset scenario')
        #Sleep introduced so that the concurrently running generate_sqls gets time 
        #to actually generate some load
        tinctest.logger.info('Sleep introduced of 120 sec')
        sleep(120)
        newfault = Fault()
        if kill_all:
            newfault.kill_all_segment_processes()
        else:
            newfault.kill_all_segment_processes(False)   
        result = newfault.restart_db()
        while result.rc != 0:
            tinctest.logger.debug("gpstop -air failed - cluster restarting!!")
            tinctest.logger.debug("executing gpstop -air again...")
            result = newfault.restart_db()
        tinctest.logger.debug('Fault Injection Tests - postmaster reset scenario completed')
    
    def test_post_master_reset_during_transaction_kill_few_segment_processes(self):
        ''' Test by killing only few of the segment processes '''
        self.post_master_reset_during_transaction(kill_all = False)
    
    def test_postmaster_reset_during_transaction_kill_all_segment_processes(self):
        ''' Test by killing all the segment processes '''
        self.post_master_reset_during_transaction()

    def test_put_system_in_change_tracking_mode(self):
        ''' Putting the system in Change Tracking mode '''
        newfault = Fault()
        config = GPDBConfig()
        tinctest.logger.info('Putting the system in change tracking mode')
        newfault.kill_processes_with_role('p')
    
    def do_changetracking(self, segment_type='primary'):
        newfault = Fault()
        newfault.changetracking(segment_type)
        rtrycnt = 0
        max_rtrycnt = 20
        while (newfault.is_changetracking() == False and rtrycnt < max_rtrycnt):
            tinctest.logger.info("waiting [%s] for DB to go into change tracking" %rtrycnt)
            sleep(10)
            rtrycnt = rtrycnt + 1
        if rtrycnt == max_rtrycnt:
            tinctest.logger.error("Segments not up after incremental recovery!!")
            return False
        return True
    
    def test_changetracking_kill_primary_reboot(self):
        sleep(30) #sleep 30sec, so that there will be some inflight Transactions
        is_ct_mode_enabled = self.do_changetracking()
        if not is_ct_mode_enabled:
            self.fail("Change Tracking mode not enabled!!")
        sleep(100) #sleep 100sec, so that there will be some inflight Transactions 
        #Reboot so that concurrent sessions will end
        newfault = Fault()
        newfault.restart_db()

    def test_changetracking_kill_primary(self):
        sleep(30) #sleep 30sec, so that there will be some inflight Transactions
        is_ct_mode_enabled = self.do_changetracking()
        if not is_ct_mode_enabled:
            self.fail("Change Tracking mode not enabled!!")
        newfault = Fault()
        sleep(100)
        tinctest.logger.info('Executing incremental recovery in the background')
        newfault._run_sys_cmd('gprecoverseg -a &')
        newfault.stop_db('i')
        
    def test_kill_segments_periodically(self):
        ''' Create system faults by killing segments randomly  '''
        newfault = Fault()
        #1. Kill Mirror Segments
        newfault.kill_segment_processes_randomly('m')
        #Sleep 10-15 min
        ntime = randint(600, 900)
        tinctest.logger.info('sleep for %s sec. before start killing primary process' %ntime)
        sleep(ntime)
        #3. Kill primary Process
        newfault.kill_segment_processes_randomly('p')
        #Sleep 10-15 min
        ntime = randint(600, 900)
        tinctest.logger.info('sleep for %s sec. befor reboot' %ntime)
        sleep(ntime)
        #5. Restart the cluster
        newfault.restart_db()
         
    def crash_system_after_checkpoint(self, mode):
        '''Issue checkpoint followed by gpstop -ai immediately '''
        newfault = Fault()
        if mode == 'InSync':
            #Sleep 10-15 min
            ntime =  randint(600, 900)
            tinctest.logger.info('sleep for %s sec...' %ntime)
            sleep(ntime)
        else:
            #Sleep 10-15 min
            ntime =  randint(600, 900)
            tinctest.logger.info('sleep for %s sec...' %ntime)
            sleep(ntime)
            
            #put system in CT
            is_ct_mode_enabled = self.do_changetracking()
            if not is_ct_mode_enabled:
                self.fail("Change Tracking mode not enabled!!")
            
            #Sleep 10-15 min
            ntime =  randint(600, 900)
            tinctest.logger.info('sleep for %s sec...' %ntime)
            sleep(ntime)

        #issue checkpoint
        newfault.issue_checkpoint()
        newfault.restart_db()
        
    def test_crash_after_checkpoint_insync(self):
        self.crash_system_after_checkpoint('InSync')
        
    def test_crash_after_checkpoint_ct(self):
        self.crash_system_after_checkpoint('CT')         

class PartitionTableScenario(TINCTestCase):
    '''Issues checkpoint and restarts the DB in a new session after the create partition table session is executed'''
    
    def test_partition_table_scenario(self):
        tinctest.logger.debug('Starting the Partition tables scenario')
        newfault = Fault()
        if newfault.issue_checkpoint():
            tinctest.logger.info('Checkpoint forced..')
        else:
            tinctest.logger.info('Unable to force checkpoint - Is the server running?')
            self.fail('Unable to force checkpoint - Check the server')
        newfault.restart_db()
        tinctest.logger.debug('Partition tables scenario completed')
        
class OOMScenario(TINCTestCase):
    '''Restarts the DB after a wait period of 40 mins '''
        
    def test_oom_scenario(self):
        tinctest.logger.debug('Restarting the db in 40 mins for hitting OOM')
        newfault = Fault()
        # Sleep for 40 minutes so that the OOM scenario is hit before restarting the system
        sleep(2400)
        newfault.restart_db()
        
        
class CheckpointTest(TINCTestCase):
    def test_skip_checkpoint(self):
        tinctest.logger.info('Skipping the checkpoint...')
        newfault = Fault()
        newfault.skip_checkpoint()
              
'''
System recovery
'''
class RecoveryTest(TINCTestCase):

    def check_db(self):
        checkDBUp()

    def test_recovery(self):
        gprecover = GpRecover()
        gprecover.incremental()
        gprecover.wait_till_insync_transition()
            
    def test_recovery_full(self):
        gprecover = GpRecover()
        gprecover.full()
        gprecover.wait_till_insync_transition()
   
    def test_rebalance_segment(self):
        newfault = Fault()
        self.assertTrue(newfault.rebalance_cluster(),"Segments not rebalanced!!")
            
    def test_recovery_abort(self):
        newfault = Fault()
        sleep(100)
        newfault._run_sys_cmd('gprecoverseg -a &')
        newfault.stop_db('i')

    def test_recovery_full_abort(self):
        newfault = Fault()
        sleep(100)
        newfault._run_sys_cmd('gprecoverseg -aF &')
        newfault.stop_db('i')

'''
   DB Opration
'''
class GPDBdbOps(TINCTestCase):

    def gpstop_db(self):
        ''' Stop database with normal mode '''
        newfault = Fault()
        sleep(5)
        newfault.stop_db()
        
    def gpstop_db_immediate(self):
        ''' Stop database with immediate mode '''
        newfault = Fault()
        sleep(5)
        newfault.stop_db('i')
        
    def gpstart_db(self):
        ''' Start Database with normal mode '''
        sleep(5)
        newfault = Fault()
        newfault.start_db()
        
    def gprestart_db(self):
        ''' Restarts the Database '''
        sleep(5)
        newfault = Fault()
        newfault.restart_db()
        
    def gpstart_db_restricted(self):
        ''' Start Database with Restricted mode '''
        sleep(5)
        newfault = Fault()
        newfault.start_db('R')
        
'''
   Test : Fault test for drop_db utility 
'''
class DropDBTest(TINCTestCase):

    def test_drop_db(self):
        ''' Test to Simulate drop_db  when database dir is offline '''
        newfault = Fault()
        sleep(20)
        #Get segment host, data dir, and DB oid to create fault
        (hostname, dirloc, oid) =  newfault.get_host_dir_oid()

        # Move database dir to take it offline
        fromDir = dirloc + '/base/'+ oid
        toDir = dirloc + '/base/../../'
        newfault.move_dir(hostname, fromDir, toDir)                
        newfault.drop_db()

        #restore database dir
        fromDir = dirloc + '/base/../../'+ oid
        toDir = dirloc + '/base/'
        newfault.move_dir(hostname, fromDir, toDir)
        newfault.drop_db()
        
        #Create db so that validations will work
        newfault.create_db()
        
    def test_drop_db_dir(self):
        ''' Drop Database dir only '''
        newfault = Fault()
        #Get segment host, data dir, and DB oid to create fault
        (hostname, dirloc, oid) =  newfault.get_host_dir_oid()

        # Move database dir to take it offline
        DirPath = dirloc + '/base/'+ oid
        newfault.drop_dir(hostname, DirPath)
        #issue checkpoint
        newfault.issue_checkpoint()
 
