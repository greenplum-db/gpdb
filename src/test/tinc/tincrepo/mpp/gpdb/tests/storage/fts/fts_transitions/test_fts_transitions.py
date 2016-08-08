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

Data Corruption test suite, to test the Data Consistency ability
of GPDB through various faults / panics by leveraging the fault
injector utility.

Cluster should have mirror configured.
"""

import tinctest
from time import sleep

from mpp.models import MPPTestCase
from tinctest.lib import local_path, run_shell_command
from gppylib.commands.base import Command
from storage.fts.fts_transitions import FTSUtil
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.gpdb.tests.storage.lib.common_utils import Gpstate
from mpp.gpdb.tests.storage.lib.common_utils import checkDBUp
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop
from mpp.lib.PSQL import PSQL

class Fts_transition(MPPTestCase):
    
    def __init__(self,methodName):
        self.ftsUtil = FTSUtil()
        self.gpstate = Gpstate()
        self.filerepUtil = Filerepe2e_Util()
        self.gprecover = GpRecover()
        self.gpstop = GpStop()
        self.gpstart = GpStart()
        super(Fts_transition,self).__init__(methodName)
    
    def setUp(self):
        out_file=local_path('reset_fault_primary_mirror.out')
        self.filerepUtil.inject_fault(f='all', m='async', y='reset', r='primary_mirror', H='ALL', outfile=out_file)

    def tearDown(self):
        pass

    def doTest(self, file):
        """
        To Execute the test case
        """
        PSQL.run_sql_file(sql_file = local_path(file))
        tinctest.logger.info( "\n Done executing %s" %(file))

    def postmaster_reset_test_validation(self,phase,type):
        PSQL.run_sql_file(local_path('fts_test_ddl_dml.sql'))
        self.gprecover.wait_till_insync_transition()
        self.gpstate.run_gpstate(type,phase)
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def fts_test_run(self,phase,type):
        tinctest.logger.info( "\n Done Injecting Fault")
        PSQL.run_sql_file(local_path('test_ddl.sql'))
        self.filerepUtil.wait_till_change_tracking_transition()
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate(type,phase)
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    # Johnny Soedomo
    # 10/20/11
    # Updated the behavior. After postmaster reset on mirror, GPDB went into ct.
    # Run gprecoverseg and wait until in sync.
    def test_01_mirror_sync_postmaster_reset_filerep_sender(self):
        ''' Test Case 1a 1: Sync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_sender panic on Mirror'''
        out_file=local_path('test_01_mirror_sync_postmaster_reset_filerep_sender.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')
    
    # Johnny Soedomo
    # 10/20/11
    # Updated the behavior. After postmaster reset on mirror, GPDB went into ct.
    # Run gprecoverseg and wait until in sync.
    def test_02_mirror_sync_postmaster_reset_filerep_receiver(self):
        ''' Test Case 1a 2: Sync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_receiver panic on Mirror'''
        out_file=local_path('test_02_mirror_sync_postmaster_reset_filerep_receiver.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    # Johnny Soedomo
    # 10/20/11
    # Updated the behavior. After postmaster reset on mirror, GPDB went into ct.
    # Run gprecoverseg and wait until in sync.
    def test_03_mirror_sync_postmaster_reset_filerep_flush(self):
        ''' Test Case 1a 3: Sync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_flush panic on Mirror'''
        out_file=local_path('test_03_mirror_sync_postmaster_reset_filerep_flush.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    # Johnny Soedomo
    # 10/20/11
    # Updated the behavior. After postmaster reset on mirror, GPDB went into ct.
    # Run gprecoverseg and wait until in sync.
    def test_04_mirror_sync_postmaster_reset_filerep_consumer(self):
        ''' Test Case 1a 4: Sync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_consumer panic on Mirror'''
        out_file=local_path('test_04_mirror_sync_postmaster_reset_filerep_consumer.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    # Johnny Soedomo
    # 10/20/11
    # GPDB is now in sync
    def test_05_primary_sync_postmaster_reset_filerep_sender(self):
        ''' Test Case 1b 1: Sync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_sender panic on Primary'''
        out_file=local_path('test_05_primary_sync_postmaster_reset_filerep_sender.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.postmaster_reset_test_validation('sync1','mirror')

    # GPDB is still in sync
    def test_06_primary_sync_postmaster_reset_filerep_receiver(self):
        ''' Test Case 1b 2: Sync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_receiver panic on Primary'''
        out_file=local_path('test_06_primary_sync_postmaster_reset_filerep_receiver.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.postmaster_reset_test_validation('sync1','mirror')

    # GPDB is still in sync
    def test_07_primary_sync_postmaster_reset_checkpoint(self):
        ''' Test Case 1b 3: Sync : Primary : non-postmaster process exits with PANIC => postmaster reset : checkpoint panic on Primary'''
        out_file=local_path('test_07_primary_sync_postmaster_reset_checkpoint.out')
        self.filerepUtil.inject_fault(f='checkpoint', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.postmaster_reset_test_validation('sync1','mirror')

    # GPDB is still in sync
    def test_08_primary_sync_postmaster_reset_filerep_flush(self):
        ''' Test Case 1b 4: Sync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_flush panic on Primary'''
        out_file=local_path('test_08_primary_sync_postmaster_reset_filerep_flush.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.postmaster_reset_test_validation('sync1','mirror')

    # GPDB is still in sync
    def test_09_primary_sync_postmaster_reset_filerep_consumer(self):
        ''' Test Case 1b 5: Sync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_consumer panic on Primary'''
        out_file=local_path('test_09_primary_sync_postmaster_reset_filerep_consumer.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.postmaster_reset_test_validation('sync1','mirror')

    def test_10_mirror_sync_filerep_process_failover(self):
        ''' Test Case 2a: Sync : Mirror : File Rep processes missing (not exit with PANIC)=> failover to Primary'''
        out_file=local_path('test_10_mirror_sync_filerep_process_failover.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='error', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    def test_11_primary_sync_filerep_process_failover(self):
        ''' Test Case 2b: Sync : Primary : File Rep processes missing (not exit with PANIC)=> failover to Mirror'''
        out_file=local_path('test_11_primary_sync_filerep_process_failover.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='error', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    def test_12_mirror_sync_filerep_listener_failover(self):
        ''' Test Case 3a: Sync : Mirror : File Rep Listener issue => failover to Primary'''
        self.ftsUtil.gpconfig_alter('mirror','true')
        cmd = Command('restart cluster','gpstop -ar')
        cmd.run()
        self.filerepUtil.wait_till_change_tracking_transition()
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.ftsUtil.gpconfig_alter('mirror','false')
        cmd = Command('restart cluster','gpstop -ar')
        cmd.run()
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()


    def test_13_primary_sync_filerep_listener_failover(self):
        ''' Test Case 3b: Sync : Primary : File Rep Listener issue => failover to Mirror'''
        self.ftsUtil.gpconfig_alter('primary','true')
        cmd = Command('restart cluster','gpstop -ar')
        cmd.run()
        self.filerepUtil.wait_till_change_tracking_transition()
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.ftsUtil.gpconfig_alter('primary','false')
        cmd = Command('restart cluster','gpstop -ar')
        cmd.run()
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_14_mirror_sync_dealock_failover(self):
        ''' Test Case 4a: Sync : Mirror : deadlock in File Rep protocol => failover to Primary'''
        out_file=local_path('test_14_mirror_sync_dealock_failover.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='infinite_loop', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    def test_15_primary_sync_deadlock_failover(self):
        ''' Test Case 4b: Sync : Primary : deadlock in File Rep protocol => failover to Mirror'''
        out_file=local_path('test_15_primary_sync_deadlock_failover.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='infinite_loop', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    def test_16_primary_sync_filerep_network_failover(self):
        ''' Test Case 5: Sync : Primary : File Rep Network issue => failover to Primary'''
        out_file=local_path('test_16_primary_sync_filerep_network_failover.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    def test_17_primary_sync_backend_process(self):
        ''' MPP-11612 - Test Case 6a: Sync : Primary : backend processes exits => transaction gets aborted - (i) IO related system call failures from backend processes'''
        self.skipTest('Unvalid test case')
        out_file=local_path('test_17_primary_sync_backend_process.out')
        self.filerepUtil.inject_fault(f='start_prepare', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    def test_18_primary_sync_process_missing_failover(self):
        ''' Test Case 7: Sync : Primary : postmaster process missing or not accessible => failover to mirror'''
        out_file=local_path('test_18_primary_sync_process_missing_failover.out')
        self.filerepUtil.inject_fault(f='postmaster', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')
        cmd = Command('restart cluster','gpstop -ar')
        cmd.run()

    def test_19_primary_sync_system_call_failover(self):
        ''' Test Case 8: Sync : Primary : system call failures from IO operations from filerep processes (resync workers issue read to primary) => failover to primary'''
        self.skipTest('Unvalid test case')
        out_file=local_path('test_19_primary_sync_system_call_failover.out')
        self.filerepUtil.inject_fault(f='filerep_resync_worker_read', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.filerepUtil.wait_till_change_tracking_transition()
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_20_primary_sync_mirror_cannot_keepup_failover(self):
        ''' Test Case 9: Sync : Primary : mirror cannot keep up with primary => failover to Primary'''
        out_file=local_path('test_20_primary_sync_mirror_cannot_keepup_failover.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        # trigger the transition to change tracking
        outfile=local_path('fault_testcase9_filerep_receiver_suspend_on_primary_trigger_ct_transition.out')
        PSQL.run_sql_command('drop table if exists bar; create table bar(i int);', background=True)
        #gp_segment_connect_timeout=600 by default, need a little more time than that to complete the transition to ct
        sleep(1000)
        out_file=local_path('test_20_primary_sync_mirror_cannot_keepup_failover_resume_fault.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        self.filerepUtil.wait_till_change_tracking_transition()
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_21_change_tracking_transition_failover(self):
        ''' Test Case 13: Change Tracking : Change Tracking Failure during transition to change tracking => postmaster reset '''
        out_file=local_path('test_21_change_tracking_transition_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        out_file=local_path('test_21_change_tracking_transition_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_transition_to_change_tracking', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    def test_22_change_tracking_failure_crashrecovery(self):
        ''' Test Case 14: Change Tracking : Change Tracking Failure during crash recovery in change tracking => postmaster reset'''
        self.skipTest('Skip for now')
        out_file=local_path('test_22_change_tracking_failure_crashrecovery_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        # trigger the transition to change tracking
        PSQL.run_sql_command('drop table if exists bar; create table bar(i int);')
        outfile=local_path('test_22_change_tracking_failure_crashrecovery_2.out')
        command = "gpconfig -c filerep_inject_change_tracking_recovery_fault -v true --skipvalidation > %s 2>&1" % (outfile)
        run_shell_command(command)
        cmd = Command('restart cluster','gpstop -ar')
        cmd.run()
        # set the guc back
        command = "gpconfig -c filerep_inject_change_tracking_recovery_fault -v false --skipvalidation > %s 2>&1" % (outfile)
        run_shell_command(command)
        self.ftsUtil.kill_postgres_process_all() 
        cmd = Command('start cluster','gpstart -a')
        cmd.run()
        self.fts_test_run('ct','primary')

    # Johnny Soedomo
    # 10/20/11
    # Updated Test Case
    # Mirror down, Primary went to ChangeTracking
    def test_23_mirror_resync_postmaster_reset_filerep_sender(self):
        ''' Test Case 14a 1: ReSync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_sender panic on Mirror'''
        out_file=local_path('test_23_mirror_resync_postmaster_reset_filerep_sender_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('fault_testcase14_a_1_suspend_resync.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        out_file=local_path('test_23_mirror_resync_postmaster_reset_filerep_sender_2.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        out_file=local_path('fault_testcase14_a_1_resume_resync.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    # Johnny Soedomo
    # 10/20/11
    # Updated Test Case
    # Primary in C, Mirror in R and D
    def test_24_mirror_resync_postmaster_reset_filerep_receiver(self):
        ''' Test Case 14a 2: ReSync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_receiver panic on Mirror'''
        out_file=local_path('test_24_mirror_resync_postmaster_reset_filerep_receiver_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('fault_testcase14_a_2_suspend_resync.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        out_file=local_path('test_24_mirror_resync_postmaster_reset_filerep_receiver_2.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        out_file=local_path('test_24_mirror_resync_postmaster_reset_filerep_receiver_3.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    # Johnny Soedomo
    # 10/20/11
    # Updated Test Case
    # Primary in C, Mirror in R and D
    def test_25_mirror_resync_postmaster_reset_filerep_flush(self):
        ''' Test Case 14a 3: ReSync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_flush panic on Mirror'''
        out_file=local_path('test_25_mirror_resync_postmaster_reset_filerep_flush_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('fault_testcase14_a_3_suspend_resync.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        out_file=local_path('test_25_mirror_resync_postmaster_reset_filerep_flush_2.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        out_file=local_path('test_25_mirror_resync_postmaster_reset_filerep_flush_3.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    # Cannot connect to GPDB, DTM, able to restart GPDB and gprecoverseg
    # MPP-12402
    # Johnny Soedomo
    # 10/20/11
    # Now passes with latest MAIN only on OSX, same issue with RH
    def test_26_primary_resync_postmaster_reset_filerep_receiver(self):
        ''' Test Case 14b 2: ReSync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_receiver panic on Primary'''
        self.skipTest('Known Issue') # Known Issue
        out_file=local_path('test_26_primary_resync_postmaster_reset_filerep_receiver_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_26_primary_resync_postmaster_reset_filerep_receiver_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_26_primary_resync_postmaster_reset_filerep_receiver_3.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        out_file=local_path('test_26_primary_resync_postmaster_reset_filerep_receiver_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','up')
        self.ftsUtil.check_mastermirrorintegrity()

    # This works
    def test_27_primary_resync_postmaster_reset_checkpoint(self):
        ''' Test Case 14b 3: ReSync : Primary : non-postmaster process exits with PANIC => postmaster reset : checkpoint panic on Primary'''
        out_file=local_path('test_27_primary_resync_postmaster_reset_checkpoint_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_27_primary_resync_postmaster_reset_checkpoint_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_27_primary_resync_postmaster_reset_checkpoint_3.out')
        self.filerepUtil.inject_fault(f='checkpoint', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        out_file=local_path('test_27_primary_resync_postmaster_reset_checkpoint_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','up')
        self.ftsUtil.check_mastermirrorintegrity()

    # Cannot connect to GPDB, DTM, database won't come up even restarting
    # MPP-12402
    # Johnny Soedomo
    # 10/20/11
    # Now passes with latest MAIN, Pass on both OSX and RH (VM), however killnz0 (physical machine) does not work
    def test_28_primary_resync_postmaster_reset_filerep_flush(self):
        ''' Test Case 14b 4: ReSync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_flush panic on Primary'''
        self.skipTest('Known Issue') # Known Issue
        out_file=local_path('test_28_primary_resync_postmaster_reset_filerep_flush_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_28_primary_resync_postmaster_reset_filerep_flush_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_28_primary_resync_postmaster_reset_filerep_flush_3.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        out_file=local_path('test_28_primary_resync_postmaster_reset_filerep_flush_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','up')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_29_primary_resync_postmaster_reset_filerep_consumer(self):
        ''' Test Case 14b 5: ReSync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_consumer panic on Primary'''
        out_file=local_path('test_29_primary_resync_postmaster_reset_filerep_consumer_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file) 
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_29_primary_resync_postmaster_reset_filerep_consumer_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_29_primary_resync_postmaster_reset_filerep_consumer_3.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='reset', r='primary', H='ALL', outfile=out_file)        
        out_file=local_path('test_29_primary_resync_postmaster_reset_filerep_consumer_4.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        out_file=local_path('test_29_primary_resync_postmaster_reset_filerep_consumer_5.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','up')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_30_mirror_resync_process_missing_failover(self):
        ''' Test Case 15a: ReSync : Mirror : File Rep processes missing (not exit with PANIC)=> failover to Primary'''
        out_file=local_path('test_30_mirror_resync_process_missing_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_30_mirror_resync_process_missing_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_30_mirror_resync_process_missing_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='error', r='mirror', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_sender error on mirror")
        out_file=local_path('test_30_mirror_resync_process_missing_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)       
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()


    def test_31_primary_resync_process_missing_failover(self):
        ''' Test Case 15b: ReSync : Primary : File Rep processes missing (not exit with PANIC)=> failover to Mirror'''
        out_file=local_path('test_31_primary_resync_process_missing_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_31_primary_resync_process_missing_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('primary','resync_incr')
        out_file=local_path('test_31_primary_resync_process_missing_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='reset', r='primary', H='ALL', outfile=out_file)
        out_file=local_path('test_31_primary_resync_process_missing_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='error', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_sender error on primary")
        out_file=local_path('test_31_primary_resync_process_missing_failover_5.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    # Primary and Mirror are in resync, seems slow
    # Does not create, insert table, maybe revisit this test
    # failed to gprecoverseg, need to gprecoverseg again
    def test_32_mirror_resync_deadlock_failover(self):
        ''' Test Case 17a: ReSync : Mirror : deadlock in File Rep protocol => failover to Primary'''
        out_file=local_path('test_32_mirror_resync_deadlock_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_32_mirror_resync_deadlock_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_32_mirror_resync_deadlock_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='infinite_loop', r='mirror', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_sender_infinite_loop_on_mirror")
        out_file=local_path('test_32_mirror_resync_deadlock_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()


    def test_33_primary_resync_deadlock_failover(self):
        ''' Test Case 17b: ReSync : Primary : deadlock in File Rep protocol => failover to Mirror'''
        out_file=local_path('test_33_primary_resync_deadlock_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file) 
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_33_primary_resync_deadlock_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('primary','resync_incr')
        out_file=local_path('test_33_primary_resync_deadlock_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='infinite_loop', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_sender_infinite_loop_on_primary")
        out_file=local_path('test_33_primary_resync_deadlock_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_34_primary_resync_filerep_network_failover(self):
        ''' Test Case 18: ReSync : Primary : File Rep Network issue => failover to Primary'''
        self.skipTest('Known Issue')
        out_file=local_path('test_34_primary_resync_filerep_network_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_34_primary_resync_filerep_network_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_34_primary_resync_filerep_network_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_consumer panic on mirror")
        out_file=local_path('test_34_primary_resync_filerep_network_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    # This error
    # Primary in C, Mirror in R and D
    def test_35_primary_resync_backend_process(self):
        ''' Test Case 19a: ReSync : Primary : backend processes exits => transaction gets aborted - (i) IO related system call failures from backend processes'''
        self.skipTest('Known Issue')
        out_file=local_path('test_35_primary_resync_backend_process_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_35_primary_resync_backend_process_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_35_primary_resync_backend_process_3.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='error', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_flush error on primary")
        out_file=local_path('test_35_primary_resync_backend_process_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_36_primary_resync_postmaster_missing_failover(self):
        ''' Test Case 20: ReSync : Primary : postmaster process missing or not accessible => failover to mirror'''
        out_file=local_path('test_36_primary_resync_postmaster_missing_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_36_primary_resync_postmaster_missing_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_36_primary_resync_postmaster_missing_failover_3.out')
        self.filerepUtil.inject_fault(f='postmaster', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault postmaster_panic_on_primary")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        cmd = Command('restart', 'gpstop -ar')
        cmd.run()
        self.gprecover.incremental()
        self.doTest('fts_test_ddl_dml.sql')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_37_primary_resync_system_failover(self):
        ''' Test Case 21: ReSync : Primary : system call failures from IO operations from filerep processes (resync workers issue read to primary) => failover to primary'''
        out_file=local_path('test_37_primary_resync_system_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_37_primary_resync_system_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_37_primary_resync_system_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_resync_worker_read', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_resync_worker_read_fault_on_primary")
        out_file=local_path('test_37_primary_resync_system_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('mirror','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_38_primary_resync_mirror_cannot_keepup_failover(self):
        ''' Test Case 22: ReSync : Primary : mirror cannot keep up with primary => failover to Primary'''
        out_file=local_path('test_38_primary_resync_mirror_cannot_keepup_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_38_primary_resync_mirror_cannot_keepup_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('mirror','resync_incr')
        out_file=local_path('test_38_primary_resync_mirror_cannot_keepup_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='sleep', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_receiver_sleep_on_primary")
        out_file=local_path('test_38_primary_resync_mirror_cannot_keepup_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_39_mirror_resync_filerep_network(self):
        ''' Test Case 23: ReSync : Mirror : FileRep Network issue => fts prober follows primary segment rules to make decision about where to failover'''
        out_file=local_path('test_39_mirror_resync_filerep_network_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_39_mirror_resync_filerep_network_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('primary','resync_incr')
        out_file=local_path('test_39_mirror_resync_filerep_network_3.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='fault', r='mirror', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_receiver_fault_on_mirror")
        out_file=local_path('test_39_mirror_resync_filerep_network_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_40_mirror_resync_system_failover(self):
        ''' Test Case 24: ReSync : Mirror : system call failures from IO operations => failover to primary '''
        out_file=local_path('test_40_mirror_resync_system_failover_1.out') 
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_40_mirror_resync_system_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('primary','resync_incr')
        out_file=local_path('test_40_mirror_resync_system_failover_3.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='fault', r='mirror', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault filerep_flush_fault_on_mirror")
        out_file=local_path('test_40_mirror_resync_system_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    def test_41_mirror_resync_postmaster_missing_failover(self):
        ''' Test Case 25: ReSync : Mirror : postmaster process missing or not accessible => failover to primary '''
        out_file=local_path('test_41_mirror_resync_postmaster_missing_failover_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_41_mirror_resync_postmaster_missing_failover_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to suspend resync")
        self.gprecover.incremental()
        self.gpstate.run_gpstate('primary','resync_incr')
        out_file=local_path('test_41_mirror_resync_postmaster_missing_failover_3.out')
        self.filerepUtil.inject_fault(f='postmaster', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault postmaster_panic_on_mirror")
        out_file=local_path('test_41_mirror_resync_postmaster_missing_failover_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to resume resync")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()

    # Modified: Johnny Soedomo
    # Moved to the end of the test. This used to be testQuery34
    def test_42_mirror_sync_filerep_network(self):
        ''' Test Case 10: Sync : Mirror : FileRep Network issue => fts prober follows primary segment rules to make decision about where to failover'''
        out_file=local_path('test_42_mirror_sync_filerep_network.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='fault', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    # Modified: Johnny Soedomo
    # Moved to the end of the test. This used to be testQuery35
    def test_43_mirror_sync_system_io_failover(self):
        ''' Test Case 11: Sync : Mirror : system call failures from IO operations => failover to primary '''
        out_file=local_path('test_43_mirror_sync_system_io_failover.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='error', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    # Modified: Johnny Soedomo
    # Moved to the end of the test. This used to be testQuery35
    def test_44_mirror_sync_postmaster_missing_failover(self):
        ''' Test Case 12: Sync : Mirror : postmaster process missing or not accessible => failover to primary '''
        out_file=local_path('test_44_mirror_sync_postmaster_missing_failover.out')
        self.filerepUtil.inject_fault(f='postmaster', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','mirror')

    # Johnny Soedomo
    # 10/20/11
    # Updated Test Case
    # Primary in C, Mirror in S and D
    def test_45_mirror_resync_postmaster_reset_filerep_consumer(self):
        ''' Test Case 14a 4: ReSync : Mirror : non-postmaster process exits with PANIC => postmaster reset : filerep_consumer panic on Mirror'''
        out_file=local_path('test_45_mirror_resync_postmaster_reset_filerep_consumer_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_45_mirror_resync_postmaster_reset_filerep_consumer_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file) 
        self.gprecover.incremental()
        out_file=local_path('test_45_mirror_resync_postmaster_reset_filerep_consumer_3.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='panic', r='mirror', H='ALL', outfile=out_file)
        out_file=local_path('test_45_mirror_resync_postmaster_reset_filerep_consumer_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')



    def test_46_primary_resync_postmaster_reset_filerep_sender(self):
        ''' Test Case 14b 1: ReSync : Primary : non-postmaster process exits with PANIC => postmaster reset : filerep_sender panic on Primary'''
        out_file=local_path('test_46_primary_resync_postmaster_reset_filerep_sender_1.out')
        self.filerepUtil.inject_fault(f='filerep_consumer', m='async', y='fault', r='primary', H='ALL', outfile=out_file)
        tinctest.logger.info( "\n Done Injecting Fault to put to change tracking")
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.filerepUtil.wait_till_change_tracking_transition()
        out_file=local_path('test_46_primary_resync_postmaster_reset_filerep_sender_2.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='suspend', r='primary', H='ALL', outfile=out_file)
        self.gprecover.incremental()
        self.gpstate.run_gpstate('primary','resync_incr')
        out_file=local_path('test_46_primary_resync_postmaster_reset_filerep_sender_3.out')
        self.filerepUtil.inject_fault(f='filerep_sender', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        out_file=local_path('test_46_primary_resync_postmaster_reset_filerep_sender_4.out')
        self.filerepUtil.inject_fault(f='filerep_resync', m='async', y='resume', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.doTest('fts_test_ddl_dml.sql')
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','up')
        self.ftsUtil.check_mastermirrorintegrity()


    def test_postmaster_reset_mpp13971(self):
        ''' FTS MPP-13971: Postmaster reset fails on mirror, transition is not copied to local memory.'''
        out_file=local_path('test_postmaster_reset_mpp13971.out')
        self.filerepUtil.inject_fault(f='filerep_flush', m='async', y='panic', r='primary', H='ALL', outfile=out_file)
        PSQL.run_sql_file(local_path('test_ddl.sql'),'-a')
        self.postmaster_reset_test_validation('sync1','mirror')

    def test_postmaster_reset_mpp13689(self):
        ''' FTS MPP-13689: Segment fails to restart database processes when transitioned to change-tracking during a postmaster reset. '''
        out_file=local_path('test_postmaster_reset_mpp13689.out')
        self.filerepUtil.inject_fault(f='filerep_receiver', m='async', y='fatal', r='mirror', H='ALL', outfile=out_file)
        self.fts_test_run('ct','primary')

    def test_postmaster_reset_mpp14506(self):
        ''' FTS MPP-14506: FTS hangs if postmaster reset occurs on the master during transitioning a segment to change-tracking'''
        out_file=local_path('test_postmaster_reset_mpp14506.out')
        # Only run this if gp_fts_transition_parallel is ON
        # if guc == "gp_fts_transition_parallel":
        self.filerepUtil.inject_fault(f='segment_transition_request', m='async', y='infinite_loop', r='primary', H='ALL', outfile=out_file) 
        tinctest.logger.info( "\n Done Injecting Fault")
        # Kill Mirror
        self.ftsUtil.killFirstMirror()
        self.filerepUtil.wait_till_change_tracking_transition()
        # Kill Master "writer" process
        self.ftsUtil.killMastersProcess(ProcName='writer process')
        checkDBUp()
        self.gpstate.run_gpstate('primary','ct')
        self.gprecover.incremental()
        self.gprecover.wait_till_insync_transition()
        self.ftsUtil.check_mirrorintegrity('True','normal')
        self.ftsUtil.check_mastermirrorintegrity()
