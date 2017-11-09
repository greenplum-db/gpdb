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
import glob
from time import sleep
import tinctest
from tinctest.lib  import local_path
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL

from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop
from mpp.lib.config import GPDBConfig
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.gpdbverify import GpdbVerify
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.gpdb.tests.storage.lib.common_utils import *

class PgtwoPhaseClass(MPPTestCase):
    '''Helper class for pg_twophase supporting functions '''

    def __init__(self,methodName):
        self.filereputil = Filerepe2e_Util()
        self.config = GPDBConfig()
        self.gprecover = GpRecover(self.config)
        self.gpstop = GpStop()
        self.gpstart = GpStart()
        self.gpfile = Gpfilespace(self.config)
        self.gpverify = GpdbVerify(config=self.config)
        self.dbstate = DbStateClass('run_validation',self.config)
        self.port = os.getenv('PGPORT')
        super(PgtwoPhaseClass,self).__init__(methodName)

    def invoke_fault(self, fault_name, type, role='mirror', port=None, occurence=None, sleeptime=None, seg_id=None):
        ''' Reset the fault and then issue the fault with the given type'''
        self.filereputil.inject_fault(f=fault_name, y='reset', r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        self.filereputil.inject_fault(f=fault_name, y=type, r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        tinctest.logger.info('Successfully injected fault_name : %s fault_type : %s' % (fault_name, type))

    def inject_fault(self, fault_type):
        '''
        @param fault_type : type of fault to ne suspended
        '''
        if fault_type == 'end_prepare_two_phase_sleep':
            self.filereputil.inject_fault(f='end_prepare_two_phase_sleep', sleeptime='1000', y='sleep', r='primary', p=self.port)
            tinctest.logger.info('Injected fault to sleep in end_prepare_two_phase')

        elif fault_type == 'abort':
            # In case of abort fault we need to include this error type fault also, to fake a situation where one of the segment is not responding back, which can make the master to trigger an abort transaction
            self.invoke_fault('transaction_abort_after_distributed_prepared', 'error', port=self.port, occurence='0', seg_id='1')

            self.invoke_fault('twophase_transaction_abort_prepared', 'suspend', role='primary', port=self.port, occurence='0')

        elif fault_type == 'commit':
            self.invoke_fault('twophase_transaction_commit_prepared', 'suspend', role='primary', port=self.port, occurence='0')

        elif fault_type == 'dtm_broadcast_prepare':
            self.invoke_fault('dtm_broadcast_prepare', 'suspend', seg_id = '1', port=self.port, occurence='0')

        elif fault_type == 'dtm_broadcast_commit_prepared':
            self.invoke_fault('dtm_broadcast_commit_prepared', 'suspend', seg_id = '1', port=self.port, occurence='0')

        elif fault_type == 'dtm_xlog_distributed_commit':
            self.invoke_fault('dtm_xlog_distributed_commit', 'suspend', seg_id = '1', port=self.port, occurence='0')

    def resume_faults(self, fault_type, cluster_state='sync'):
        '''
        @param fault_type : commit/abort/end_prepare_two_phase_sleep/dtm_broadcast_prepare/dtm_broadcast_commit_prepared/dtm_xlog_distributed_commit
        @description : Resume the suspended faults 
        '''
        tinctest.logger.info('coming to resume faults with xact %s' % fault_type) 
        if fault_type == 'abort':
            self.filereputil.inject_fault(f='twophase_transaction_abort_prepared', y='resume', r='primary', p=self.port , o='0')
            if cluster_state !='resync':
                self.filereputil.inject_fault(f='transaction_abort_after_distributed_prepared', y='reset', p=self.port , o='0', seg_id='1')
        elif fault_type == 'commit':
            self.filereputil.inject_fault(f='twophase_transaction_commit_prepared', y='resume', r='primary', p=self.port , o='0')

        elif fault_type == 'dtm_broadcast_prepare':
            self.filereputil.inject_fault(f='dtm_broadcast_prepare', y='resume', seg_id = '1', p=self.port, o='0')

        elif fault_type == 'dtm_broadcast_commit_prepared':
            tinctest.logger.info('coming to if dtm_broadcast_commit_prepared')
            self.filereputil.inject_fault(f='dtm_broadcast_commit_prepared', y='resume', seg_id = '1', p=self.port, o='0')

        elif fault_type == 'dtm_xlog_distributed_commit':
            self.filereputil.inject_fault(f='dtm_xlog_distributed_commit', y='resume', seg_id = '1', p=self.port, o='0')

        else:
            tinctest.logger.info('No faults to resume')
        tinctest.logger.info('Resumed the suspended transaction fault')
        
        #Wait till all the trigger_sqls are complete before returning
        sql_count = PSQL.run_sql_command('select count(*) from pg_stat_activity;', flags ='-q -t', dbname='postgres')
        while(sql_count.strip() != '1'):
            sleep(5)
            sql_count = PSQL.run_sql_command('select count(*) from pg_stat_activity;', flags ='-q -t', dbname='postgres')
            tinctest.logger.info('stat_activity count %s ' % sql_count)
        return

    def start_db(self):
        '''Gpstart '''
        rc = self.gpstart.run_gpstart_cmd()
        if not rc:
            raise Exception('Failed to start the cluster')
        tinctest.logger.info('Started the cluster successfully')

    def stop_db(self):
        ''' Gpstop and dont check for rc '''
        cmd = Command('Gpstop_a', 'gpstop -a')
        tinctest.logger.info('Executing command: gpstop -a')
        cmd.run()

    def crash_and_recover(self, crash_type, fault_type, checkpoint='noskip', cluster_state='sync'):
        '''
        @param crash_type : gpstop_i/gpstop_a/failover_to_primary/failover_to_mirror
        @note: when skip checkpoint is enabled, gpstop -a returns a non-rc return code and fails in the library. To workaround, using a local function
        '''
        if crash_type == 'gpstop_i' :
            rc = self.gpstop.run_gpstop_cmd(immediate = True)
            if not rc:
                raise Exception('Failed to stop the cluster')
            tinctest.logger.info('Stopped cluster immediately')
            self.start_db()
        elif crash_type == 'gpstop_a':
            self.resume_faults(fault_type, cluster_state)
            if checkpoint == 'skip' :
                self.stop_db()
            else:
                rc = self.gpstop.run_gpstop_cmd()
                if not rc:
                    raise Exception('Failed to stop the cluster')
            tinctest.logger.info('Smart stop completed')
            self.start_db()                            
        elif crash_type == 'failover_to_primary':
            self.invoke_fault('filerep_consumer', 'fault')
            self.resume_faults(fault_type, cluster_state)
            (rc, num) =self.filereputil.wait_till_change_tracking_transition()
            tinctest.logger.info('Value of rc and num_down %s, %s, %s' % (rc, num, fault_type))

        elif crash_type == 'failover_to_mirror':
            self.invoke_fault('postmaster', 'panic', role='primary')
            if fault_type in ('dtm_broadcast_prepare', 'dtm_broadcast_commit_prepared', 'dtm_xlog_distributed_commit') :
                self.resume_faults(fault_type, cluster_state)
            PSQL.wait_for_database_up()
            (rc, num) = self.filereputil.wait_till_change_tracking_transition()
            tinctest.logger.info('Value of rc and num_down %s, %s' % (rc, num))
            if fault_type == 'abort' :
                self.filereputil.inject_fault(f='transaction_abort_after_distributed_prepared', y='reset',p=self.port , o='0', seg_id='1')

        if cluster_state == 'resync':
            if not self.gprecover.wait_till_insync_transition():
                raise Exception('Segments not in sync')                        

    def get_trigger_status_old(self, trigger_count):
        '''Compare the pg_stat_activity count with the total number of trigger_sqls executed '''
        for i in range(1,50):
            psql_count = PSQL.run_sql_command('select count(*) from pg_stat_activity;', flags='-q -t', dbname='postgres')
        tinctest.logger.info('Count of trigger sqls %s' % psql_count)
        if int(psql_count.strip()) < trigger_count :
            tinctest.logger.info('coming to the if loop in get_trigger_status')
            return False
        return True

    def get_trigger_status(self, trigger_count, fault_type):
        if fault_type == None:
            return self.get_trigger_status_old(trigger_count);

        return self.filereputil.check_fault_status(fault_name=fault_type, status="triggered", seg_id='1', num_times_hit=trigger_count);

    def check_trigger_sql_hang(self, test_dir, fault_type = None):
        '''
        @description : Return the status of the trigger sqls: whether they are waiting on the fault 
        Since gpfaultinjector has no way to check if all the sqls are triggered, we are using 
        a count(*) on pg_stat_activity and compare the total number of trigger_sqls
        '''
        trigger_count=0
        for dir in test_dir.split(","):
            trigger_dir = local_path('%s/trigger_sql/sql/' % (dir))
            trigger_count += len(glob.glob1(trigger_dir,"*.sql"))
        tinctest.logger.info('Total number of sqls to trigger %d in %s' % (trigger_count,test_dir));
        return self.get_trigger_status(trigger_count, fault_type)


    def run_faults_before_pre(self, cluster_state):
        '''
        @param cluster_state : sync/change_tracking/resync
        @description: 1. Cluster into change_tracking in case of resync/ change_tracking. 
        '''
        if cluster_state == 'resync':
            self.invoke_fault('filerep_consumer', 'fault')
            self.filereputil.wait_till_change_tracking_transition()
            tinctest.logger.info('Change_tracking transition complete')

    def run_faults_before_trigger(self, checkpoint, cluster_state, fault_type):
        '''
        @param checkpoint : skip/noskip
        @param cluster_state : sync/change_tracking/resync
        @param fault_type : commit/abort
        @param end_prepare_two_phase_sleep : True/False
        @description : 1. Suspend resync faults. 2. Issue Checkpoint before the skip checkpoint, so that the bufferpool is cleared. 3. If skip issue 'skip checkpoint'. 4. Suspend transaction_faults based on test_type.
        '''
        if cluster_state == 'change_tracking':
            self.invoke_fault('filerep_consumer', 'fault')
            self.filereputil.wait_till_change_tracking_transition()
            tinctest.logger.info('Change_tracking transition complete')

        if cluster_state == 'resync':
            self.invoke_fault('filerep_resync', 'suspend', role='primary')

            if checkpoint == 'skip':
                self.invoke_fault('filerep_transition_to_sync_before_checkpoint', 'suspend', role='primary', port=self.port, occurence='0')
            rc = self.gprecover.incremental()
            if not rc:
                raise Exception('Gprecvoerseg failed')
            tinctest.logger.info('Cluster in resync state')

        PSQL.run_sql_command('CHECKPOINT;', dbname='postgres')
        if checkpoint == 'skip':
            self.invoke_fault('checkpoint', 'skip', role='primary', port= self.port, occurence='0')
        self.inject_fault(fault_type)

        # Can't do it after filerep_resync resume as gets stuck due to
        # filerep_transition_to_sync_before_checkpoint suspend above for
        # MirroedLock
        PSQL.wait_for_database_up()

        if cluster_state == 'resync':
            self.filereputil.inject_fault(f='filerep_resync', y='resume', r='primary')

    def run_crash_and_recover(self, crash_type, fault_type, test_dir, cluster_state='sync', checkpoint='noskip'):
        '''
        @param crash_type : gpstop_i/gpstop_a/failover_to_mirror/failover_to_primary
        @param fault_type : commit/abort/end_prepare_two_phase_sleep
        @param test_dir : dir of the trigger sqls
        @description : Execute the specified crash type before/after resuming the suspended fault and recover
        '''
        trigger_status = self.check_trigger_sql_hang(test_dir)
        tinctest.logger.info('trigger_status %s' % trigger_status)
        sleep(50) # This sleep is needed till we get a way to find the state of all suspended sqls
        if trigger_status == True:
            if cluster_state == 'resync':
                self.filereputil.inject_fault(f='filerep_transition_to_sync_before_checkpoint', y='resume', r='primary')
                sleep(15) # wait little before crash
            self.crash_and_recover(crash_type, fault_type, checkpoint, cluster_state)
        else:
            tinctest.logger.info('The fault_status is not triggered')
    
    def gprecover_rebalance(self):
        '''
        @description: Run rebalance through gpstop -air is much faster than gprecoverseg -r for test purpose.
        '''
        rc = self.gpstop.run_gpstop_cmd(immediate = True)
        if not rc:
           raise Exception('Failed to stop the cluster')
        tinctest.logger.info('Stopped cluster immediately')
        self.start_db()

    def run_gprecover(self, crash_type, cluster_state='sync'):
        '''Recover the cluster if required. '''
        if crash_type in ('failover_to_primary', 'failover_to_mirror') or cluster_state == 'change_tracking' :
            rc = self.gprecover.incremental()
            if not rc:
                raise Exception('Gprecvoerseg failed')
            if not self.gprecover.wait_till_insync_transition():
                raise Exception('Segments not in sync')                        
            tinctest.logger.info('Cluster in sync state')
            if crash_type == 'failover_to_mirror' :
                self.gprecover_rebalance()
                tinctest.logger.info('Successfully Rebalanced the cluster')
        else:
            tinctest.logger.info('No need to run gprecoverseg. The cluster should be already in sync')


    def switch_ckpt_faults_before_trigger(self, cluster_state, fault_type):
        '''
        @param cluster_state : sync/change_tracking/resync
        @param fault_type : dtm_broadcast_prepare/dtm_broadcast_commit_prepared/dtm_xlog_distributed_commit
        '''
        if cluster_state in ('change_tracking', 'resync'):
            self.invoke_fault('filerep_consumer', 'fault')
            self.filereputil.wait_till_change_tracking_transition()
            tinctest.logger.info('Change_tracking transition complete') 

        if cluster_state == 'resync':
            self.invoke_fault('filerep_resync', 'suspend', role='primary')
            rc = self.gprecover.incremental()
            if not rc:
                raise Exception('Gprecvoerseg failed')
            tinctest.logger.info('Cluster in resync state')
        self.inject_fault(fault_type)

    def switch_ckpt_switch_xlog(self):
        '''
        @description: pg_switch_xlog on segments
        '''
        sql_cmd = 'select * from pg_switch_xlog();'
        num_primary = self.config.get_countprimarysegments()
        for i in range(num_primary):
            (host, port) = self.config.get_hostandport_of_segment(psegmentNumber=i)
            PSQL.run_sql_command_utility_mode(sql_cmd, host = host, port = port)

    def switch_ckpt_crash_and_recover(self, crash_type, fault_type, test_dir, cluster_state='sync', checkpoint='noskip'):
        '''
        @param crash_type : gpstop_i/gpstop_a/failover_to_mirror/failover_to_primary
        @param fault_type : dtm_broadcast_prepare/dtm_broadcast_commit_prepared/dtm_xlog_distributed_commit
        @param test_dir : dir of the trigger_sqls
        '''
        trigger_status = self.check_trigger_sql_hang(test_dir, fault_type)
        tinctest.logger.info('trigger_status %s' % trigger_status)

        if trigger_status == True:
            if cluster_state == 'resync':
                self.filereputil.inject_fault(f='filerep_resync', y='resume', r='primary')
                sleep(30) #Give a little time before crash.
            self.crash_and_recover(crash_type, fault_type, checkpoint, cluster_state)
        else:
            tinctest.logger.info('The fault_status is not triggered')
    
   
    def cleanup_dangling_processes(self):
        '''
        @description: Since the test suspend transactions at different stages and does immediate shutdown, 
        few processes will not be cleaned up and eventually will eat up on the system resources
        This methods takes care of killing them at the end of each test, if such processes exists
        '''

        num_primary = self.config.get_countprimarysegments()
        for i in range(num_primary):
            (host, port) = self.config.get_hostandport_of_segment(psegmentNumber=i)
            grep_cmd = "ps -ef|grep %s|grep 'Distributed'" % port
            cmd = Command('Check for dangling process', cmdStr = 'gpssh -h %s -e "%s" ' % (host, grep_cmd))
            cmd.run()
            result = cmd.get_results()
            if len(result.stdout.splitlines()) > 2 :
                grep_and_kill_cmd = "ps -ef|grep %s|grep 'Distributed'|awk '{print \$2}'|xargs kill -9" % port
                cmd = Command('Kill dangling processes', cmdStr='gpssh -h %s -e "%s" ' % (host, grep_and_kill_cmd ))
                cmd.run()
                tinctest.logger.info('Killing the dangling processes') 
