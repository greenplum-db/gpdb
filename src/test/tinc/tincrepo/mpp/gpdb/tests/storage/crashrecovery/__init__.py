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
import fileinput, re
from time import sleep
import tinctest
from tinctest.lib import local_path, Gpdiff
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop
from mpp.lib.config import GPDBConfig
from mpp.models import MPPTestCase
from mpp.lib.gpfilespace import Gpfilespace
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.gpdb.tests.storage.GPDBStorageBaseTestCase import GPDBStorageBaseTestCase


class SuspendCheckpointCrashRecovery(MPPTestCase):
    
    def __init__(self,methodName):
        self.fileutil = Filerepe2e_Util()
        self.config = GPDBConfig()
        self.gprecover = GpRecover(self.config)
        self.gpstart = GpStart()
        self.gpstop = GpStop()
        self.gpfile = Gpfilespace(self.config)
        self.dbstate = DbStateClass('run_validation', self.config)
        self.port = os.getenv('PGPORT')
        self.base = GPDBStorageBaseTestCase()
        super(SuspendCheckpointCrashRecovery,self).__init__(methodName)

    def check_system(self):
        ''' 
        @summary: Check whether the system is up and sync. Exit out if not 
        '''
        cmd ="select count(*) from gp_segment_configuration where content<> -1 ;"
        count_all = PSQL.run_sql_command(cmd, flags ='-q -t', dbname='postgres')
        cmd ="select count(*) from gp_segment_configuration where content<> -1 and mode = 's' and status = 'u';"
        count_up_and_sync = PSQL.run_sql_command(cmd, flags ='-q -t', dbname='postgres')
        if count_all.strip() != count_up_and_sync.strip() :
            os._exit(1)
        else:
            tinctest.logger.info("\n Starting New Test: System is up and in sync .........")

    def get_items_list(self, tests):
        ''' Get file contents to a list '''
        test_file = local_path(tests)
        with open(test_file, 'r') as f:
            test_list = [line.strip() for line in f]
        return test_list

    def checkPSQLRun(self, test):
        '''Check if the psql run started in background is over before running the _post.sql '''
        cmd_str = "ps -ef|grep '%s'|grep [p]sql" % test
        while(1):
            is_running = 0 
            cmd = Command('Check psql run', cmd_str)
            cmd.run()
            result = cmd.get_results()
            for line in result.stdout.splitlines():
                if '%s' %test in line:
                    tinctest.logger.info(line)
                    is_running = 1 
            if is_running == 0:
                return True
            else:
                sleep(5)
        return False

    def modify_sql_file(self, filename):
        ans_file = local_path(filename.replace('.sql' , '.ans'))
        for sfile in (filename, ans_file):
            for line in fileinput.FileInput(sfile,inplace=1):
                line = re.sub('gptest', os.getenv('PGDATABASE'), line)
                print str(re.sub('\n','',line))

    def validate_sql(self, filename):
        ''' Compare the out and ans files '''
        out_file = local_path(filename.replace(".sql", ".out"))
        ans_file = local_path(filename.replace('.sql' , '.ans'))
        assert Gpdiff.are_files_equal(out_file, ans_file)

    def run_sql(self, filename):
        ''' Run the provided sql and validate it '''
        out_file = local_path(filename.replace(".sql", ".out"))
        PSQL.run_sql_file(sql_file = filename, out_file = out_file)
        self.validate_sql(filename)

    def set_faults_before_executing_pre_sqls(self, cluster_state):
        ''' Set the checkpoint skip fault '''
        if cluster_state == 'change_tracking':
           self.cluster_in_change_tracking()
        self.fileutil.inject_fault(f='checkpoint', y='reset', r='primary', p=self.port)
        self.fileutil.inject_fault(f='checkpoint', y='skip', r='primary', p=self.port, o='0')
        tinctest.logger.info('Successfully injected fault to skip checkpointing') 
        if(cluster_state == 'resync'):
            self.fileutil.inject_fault(f='filerep_consumer', y='reset')
            self.fileutil.inject_fault(f='filerep_consumer', y='fault')
            self.fileutil.wait_till_change_tracking_transition()

    def suspend_fault(self, fault_name):
        ''' Suspend the provided fault_name '''
        self.fileutil.inject_fault(f='%s' % fault_name, y='reset', o='0', r='primary', p=self.port)
        self.fileutil.inject_fault(f='%s' % fault_name, y='suspend', o='0', r='primary', p=self.port)
        tinctest.logger.info('Successfully injected fault to suspend %s' % fault_name)

    def get_faults_before_executing_trigger_sqls(self, pass_num,cluster_state, test_type, ddl_type, aborting_create_needed=False):
        ''' Get the fault before trigger sqls are executed '''
        fault_name=''
        tinctest.logger.info('Fault Conditions: pass_num = [%s], cluster_state = [%s], test_type =  [%s], ddl_type = [%s], aborting_create_needed = [%s]' % (pass_num, cluster_state, test_type, ddl_type, aborting_create_needed)) 

        if pass_num == 1 and test_type == 'commit' and ddl_type == 'create':
            if aborting_create_needed:
                fault_name = 'finish_prepared_transaction_commit_pass1_aborting_create_needed'
            else:
                fault_name = 'finish_prepared_transaction_commit_pass1_from_create_pending_to_created'
                
        elif pass_num == 2 and test_type == 'commit' and ddl_type == 'create':
            if aborting_create_needed:
                fault_name = 'finish_prepared_transaction_commit_pass2_aborting_create_needed'
            else:
                fault_name = 'finish_prepared_transaction_commit_pass2_from_create_pending_to_created'

        elif pass_num == 1 and test_type == 'commit' and ddl_type == 'drop':
            fault_name = 'finish_prepared_transaction_commit_pass1_from_drop_in_memory_to_drop_pending'

        elif pass_num == 2 and test_type == 'commit' and ddl_type == 'drop':
            fault_name = 'finish_prepared_transaction_commit_pass2_from_drop_in_memory_to_drop_pending'

        elif pass_num == 1 and test_type == 'abort':
            if aborting_create_needed:
                fault_name = 'finish_prepared_transaction_abort_pass1_aborting_create_needed'
            else:
                fault_name = 'finish_prepared_transaction_abort_pass1_from_create_pending_to_aborting_create'

        elif pass_num == 2 and test_type == 'abort':
            if aborting_create_needed:
                fault_name = 'finish_prepared_transaction_abort_pass2_aborting_create_needed'
            else:
                fault_name = 'finish_prepared_transaction_abort_pass2_from_create_pending_to_aborting_create'

        elif pass_num == 0 and (test_type == 'abort' or test_type == 'commit'):
            pass # We already set the fault error_txn_abort_after_dist_prepare_on_master above for abort tests and for commit tests skip checkpoint is done by default for all tests.
        return fault_name

    def set_faults_before_executing_trigger_sqls(self, pass_num,cluster_state, test_type, ddl_type, aborting_create_needed=False):
        ''' Set the fault before trigger sqls are executed '''
        if (cluster_state == 'resync'):
            self.cluster_in_resync()
        fault_name=''
        fault_name = self.get_faults_before_executing_trigger_sqls(pass_num,cluster_state, test_type, ddl_type, aborting_create_needed=False);

        if (test_type == 'abort'):
            self.fileutil.inject_fault(f='transaction_abort_after_distributed_prepared', y='reset', p=self.port, o='0', seg_id=1)
            self.fileutil.inject_fault(f='transaction_abort_after_distributed_prepared', y='error', p=self.port, o='0', seg_id=1)
            tinctest.logger.info('Successfully injected fault to error out after distributed prepare for abort tests')

        if pass_num !=0 :
            self.suspend_fault(fault_name)
        elif pass_num == 0 : 
            fault_name = None
        if (cluster_state == 'resync'):
            self.fileutil.inject_fault(f='filerep_transition_to_sync_begin', y = 'reset', r = 'primary')
            self.fileutil.inject_fault(f='filerep_transition_to_sync_begin', y = 'suspend', r = 'primary')
            tinctest.logger.info('Successfully suspended filerep_transition_to_sync_begin')
            #Resume resync so that trigger sql can execute while resync is in progress
            self.fileutil.inject_fault(f='filerep_resync', y = 'resume', r = 'primary')
        return fault_name

    def cluster_in_resync(self):
        '''
        1. Suspend filerep_resync, 2. Suspend filerep_transition_to_sync_before_checkpoint, 3. Run gprecoverseg
        '''
        self.base.invoke_fault('filerep_resync', 'suspend', role='primary')
        self.base.invoke_fault('filerep_transition_to_sync_before_checkpoint', 'suspend', role='primary', port=self.port , occurence='0')
        rc = self.gprecover.incremental()
        if not rc:
            raise Exception('Gprecvoerseg failed')
        tinctest.logger.info('Cluster in resync state')

    def switch_primary_mirror_role_in_utility_mode(self):
        '''Utility routine to start the master, connect in utility mode, switch the roles of primary and mirrors and shutdown the master '''
        cmd = Command('Start master in utility mode', 'export GPSTART_INTERNAL_MASTER_ONLY=1;gpstart -m')
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            raise Exception('Unable to start master in utility mode')
        tinctest.logger.info('Started master in utility mode')
    
        sql_cmd_list = ["update gp_segment_configuration set role='t' where role ='p' and content <> -1", "update gp_segment_configuration set role='p',mode='c' where role ='m' and content <> -1", "update gp_segment_configuration set role='m',status='d' where role ='t' and content <> -1"]
        for sql_cmd in sql_cmd_list:
            PSQL.run_sql_command(sql_cmd, PGOPTIONS="-c gp_session_role=utility -c allow_system_table_mods=dml")
        tinctest.logger.info('Updated the catalog to reverse the roles')
        rc = self.gpstop.run_gpstop_cmd(masteronly = True)
        if not rc:
            raise Exception('Failure to shut down the master')

    def stop_db(self):
        ''' gpstop immediate'''
        rc = self.gpstop.run_gpstop_cmd(immediate = True)
        if not rc:
            raise Exception('Failed to stop the cluster')
        tinctest.logger.info('Stopped cluster immediately')
    
    def start_db(self, down_segments=False):
        ''' Gpstart -a '''
        rc = self.gpstart.run_gpstart_cmd()
        if not rc:
            raise Exception('Failed to start the cluster')
        tinctest.logger.info('Started the cluster successfully')
       
        if not down_segments:
            if self.config.is_down_segments():
                raise Exception('Segments got marked down')

    ''' This is sleep free version based on fault triggered status '''
    def run_crash_and_recovery_fast(self,test_dir, pass_num, cluster_state, test_type, ddl_type, aborting_create_needed=False):
        if pass_num == 0:
            self.wait_till_all_sqls_done()
        else:
            mydir=local_path(test_dir)+'/trigger_sql/sql/'
            tinctest.logger.info('mydir = %s ' % mydir)
            trigger_count = len(glob.glob1(mydir,"*trigger.sql"))
            tinctest.logger.info('*** Count of trigger : %s *** ' % (trigger_count))
            if test_dir == "abort_create_tests":
               ''' vacuum full sql don't hit the suspend fault.'''
               trigger_count = trigger_count - 1
            if test_dir == "abort_create_needed_tests":
                ''' Not all SQLs hit the fault for this case, hence wait for them to complete and then others to hit the fault'''
                self.wait_till_all_sqls_done(8 + 1)
                trigger_count = 8
            if test_dir == "abort_abort_create_needed_tests":
                ''' Not all SQLs hit the fault for this case, hence wait for them to complete and then others to hit the fault'''
                self.wait_till_all_sqls_done(6 + 1)
                trigger_count = 6
            fault_type = self.get_faults_before_executing_trigger_sqls(pass_num, cluster_state, test_type, ddl_type, aborting_create_needed=False)
            fault_hit = self.fileutil.check_fault_status(fault_name=fault_type, status="triggered", num_times_hit=trigger_count)
            if not fault_hit:
               raise Exception('Fault not hit expected number of times')

        self.stop_start_validate(cluster_state)

    def wait_till_all_sqls_done(self, count=1):
        ''' 500 here is just an arbitrarily long time "if-we-exceed-this-then-oh-crap-lets-error-out" value '''
        for i in range(1,500):
            psql_count = PSQL.run_sql_command("select count(*) from pg_stat_activity where current_query <> '<IDLE>'", flags='-q -t', dbname='postgres')
            if int(psql_count.strip()) <= count :
                return
            sleep(1)
        raise Exception('SQLs expected to complete but are still running')

    def stop_start_validate(self, cluster_state):
        ''' Do gpstop immediate, gpstart and see if all segments come back up fine '''
        if cluster_state == 'sync' :
            self.stop_db()
            self.switch_primary_mirror_role_in_utility_mode()
            tinctest.logger.info('Successfully switched roles of primary and mirrors in gp_segment_configuration')
            self.start_db(down_segments=True)
            rc = self.gprecover.incremental()
            if not rc:
                raise Exception('Gprecoverseg failed')
            if not self.gprecover.wait_till_insync_transition():
                raise Exception('Segments not in sync')
        if cluster_state == 'change_tracking':
            self.stop_db()
            self.start_db(down_segments=True)

        if cluster_state == 'resync':
            #Resume the filerep_resync filerep_transition_to_sync_begin before stop-start
            self.fileutil.inject_fault(f='filerep_transition_to_sync_begin', y='resume', r='primary')
            self.stop_db()
            self.start_db()
            if not self.gprecover.wait_till_insync_transition():
                raise Exception('Segments not in sync')
        self.dbstate.check_catalog(alldb=False)

    def cluster_in_change_tracking(self):
        '''
        Put Cluster into change_tracking
        '''
        self.base.invoke_fault('filerep_consumer', 'fault', role='primary')
        self.fileutil.wait_till_change_tracking_transition()
        tinctest.logger.info('Change_tracking transition complete')


    def validate_system(self, cluster_state):
        # Validate the system's integrity
        if (cluster_state == 'change_tracking'):
            if not self.gprecover.incremental():
                raise Exception('Gprecoverseg failed')
            if not self.gprecover.wait_till_insync_transition():
                raise Exception('Segments not in sync')
            tinctest.logger.info('Segments recovered and back in sync')

        self.dbstate.check_mirrorintegrity()
        if self.config.has_master_mirror():
            self.dbstate.check_mirrorintegrity(master=True)

    def run_fault_injector_to_skip_checkpoint(self):
        tinctest.logger.info('Skip Checkpointing using fault injector.')
        self.fileutil.inject_fault(y = 'reset', f = 'checkpoint', r ='primary', H='ALL', m ='async', o = '0', p=self.port)
        (ok, out) = self.fileutil.inject_fault(y = 'skip', f = 'checkpoint', r ='primary', H='ALL', m ='async', o = '0', p=self.port)
        if not ok:
           raise Exception('Problem with injecting fault.')

    def backup_output_dir(self,test_dir, test_id):
        indir=local_path(test_dir)
        outdir = indir+'_'+test_id
        cmdstr="cp -r "+ indir + " " + outdir
        cmd = Command(name='run cp -r ', cmdStr=cmdstr)
        tinctest.logger.info("Taking a backup of SQL directory: %s" %cmd)
        try:
            cmd.run()
        except:
            self.fail("cp -r failed.")
        tinctest.logger.info("Test SQL directory Backup Done!!")

    def do_post_run_checks(self):
        self.stop_start_validate('sync')

        rc = self.gprecover.incremental()
        if not rc:
            raise Exception('Gprecvoerseg failed')

        self.gprecover.wait_till_insync_transition()

        tinctest.logger.info("Done going from resync to insync")
        self.dbstate.check_catalog(alldb=False)
        self.dbstate.check_mirrorintegrity()

        if self.config.has_master_mirror():
            self.dbstate.check_mirrorintegrity(master=True)
