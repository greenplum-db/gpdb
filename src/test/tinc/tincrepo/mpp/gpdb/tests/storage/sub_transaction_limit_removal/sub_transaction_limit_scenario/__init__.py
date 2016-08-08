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
import os
import multiprocessing
from gppylib.commands.base import Command

from tinctest.lib import local_path, Gpdiff, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from time import sleep

from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop

from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.gpdb.tests.storage.lib import Database
from mpp.gpdb.tests.storage.lib.common_utils import checkDBUp

from mpp.lib.gpfilespace import Gpfilespace


class SubTransactionLimitRemovalTestCase(MPPTestCase):

    def __init__(self, methodName):    
        super(SubTransactionLimitRemovalTestCase,self).__init__(methodName)
   
    def check_system(self):
        '''
        @summary: Check whether the system is up and sync. Exit out if not 
        '''
        tinctest.logger.info("[STLRTest] Running check_system")   
        
        tinctest.logger.info("[STLRTest] Check whether the system is up and sync")   
        
        cmd ="select count(*) from gp_segment_configuration where content<> -1 ;"
        (num_cl) = PSQL.run_sql_command(cmd)
        count_all = num_cl.split('\n')[3].strip()
               
        cmd ="select count(*) from gp_segment_configuration where content<> -1 and mode = 's' and status = 'u';"
        (num_cl) = PSQL.run_sql_command(cmd)
        count_up_and_sync = num_cl.split('\n')[3].strip()
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)


        if count_all != count_up_and_sync :
            raise Exception("[STLRTest] System not in sync and up. Exiting test")
        else:
            tinctest.logger.info("[STLRTest] Starting New Test: System is up and in sync...")

    def clean_files(self):
        tinctest.logger.info("[STLRTest] Running clean_files")   

        PSQL.run_sql_file(local_path('drop.sql'))
        PSQL.run_sql_file(local_path('drop_filespace.sql'))
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
         

    def run_sqls(self,test):
        '''
        @summary : Run the sql 
        @param test: the sql file list
        '''        
        tinctest.logger.info("[STLRTest] Running run_sqls")   
        tinctest.logger.info("[STLRTest]Starting new thread to run sql %s"%(test))
        PSQL.run_sql_file(local_path(test))
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
   
            
    def suspend_faults(self,fault_name):
        '''
        @summary : Suspend the specified fault: reset it before issuing suspend 
        @param fault_name : Name of the fault to suspend
        '''
        tinctest.logger.info("[STLRTest] Running suspend_faults")   

        self.util = Filerepe2e_Util()

        (ok1,out1) = self.util.inject_fault(f=fault_name, m = 'async', y = 'reset', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done resetting the %s fault"%(fault_name))      

        (ok1,out1) = self.util.inject_fault(f=fault_name, m = 'async', y = 'suspend', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done suspending the %s fault"%(fault_name))
        
    def check_fault_status(self,fault_name = None, status = None, max_cycle=10):
        ''' 
        Check whether a fault is triggered. Poll till the fault is triggered
        @param name : Fault name
        @param status : Status to be checked - triggered/completed
        '''
        self.util = Filerepe2e_Util()

        tinctest.logger.info("[STLRTest] Running check_fault_status %s", status)   

        if (not fault_name) or (not status) :
            raise Exception("[STLRTest]Need a value for fault_name and status to continue")

        poll =0
        while(poll < max_cycle):
            (ok1,out1) = self.util.inject_fault(f=fault_name, m = 'async', y = 'status', r = 'primary', H ='ALL')
            poll +=1
            for line in out1.splitlines():
                if line.find(fault_name) > 0 and line.find(status) > 0 :
                    tinctest.logger.info("[STLRTest]Fault %s is %s " % (fault_name,status))
                    poll = 0
                    tinctest.logger.info("[STLRTest] Running check_fault_status %s TRUE", status)
                    return True
            tinctest.logger.info("[STLRTest] printing gp segment configuration")
            (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
            tinctest.logger.info(gp_seg_conf)

            #sleep a while before start polling again
            sleep(10)
        tinctest.logger.info("[STLRTest] Running check_fault_status %s FALSE", status)
        return False
        
        
    def filerep_fault(self,trans_state):
        '''
        @summary : Inject the filerep fault supplied
        @param trans_state : type of transition 
        '''
        tinctest.logger.info("[STLRTest] Running filerep_fault")   
        self.util = Filerepe2e_Util()

        if trans_state == 'failover_to_primary':
            tinctest.logger.info("[STLRTest] primary failover")
            (ok1,out1) = self.util.inject_fault(f='filerep_consumer', m = 'async', y = 'fault', r = 'mirror', H ='ALL')
            tinctest.logger.info("[STLRTest] printing gp segment configuration")
            (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
            tinctest.logger.info(gp_seg_conf)

            if not ok1:
                raise Exception("[STLRTest]Fault injection failed")   
            tinctest.logger.info("[STLRTest]Done primary failover fault")

        elif trans_state == 'failover_to_mirror':
            tinctest.logger.info("[STLRTest] fault for postmaster panic")
            (ok1,out1) = self.util.inject_fault(f='postmaster', m = 'async', y = 'panic', r = 'primary', H ='ALL')
            tinctest.logger.info("[STLRTest] printing gp segment configuration")
            (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
            tinctest.logger.info(gp_seg_conf)
            if not ok1:
                raise Exception("[STLRTest]Fault injection failed")   
            tinctest.logger.info("[STLRTest]Done postmaster panic fault")

        elif trans_state == 'postmaster_reset':
            tinctest.logger.info("[STLRTest] fault for filerep_sender panic")
            (ok1,out1) = self.util.inject_fault(f='filerep_sender', m = 'async', y = 'panic', r = 'primary', H ='ALL')
            tinctest.logger.info("[STLRTest] printing gp segment configuration")
            (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
            tinctest.logger.info(gp_seg_conf)
            if not ok1:
                raise Exception("[STLRTest]Fault injection failed")   
            tinctest.logger.info("[STLRTest]Done filerep_sender panic fault")
            
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        tinctest.logger.info("[STLRTest] Done Injecting Fault")

    def resume_faults(self,fault_name,trans_state):
        ''''
        @summary : Resume the fault and check status
        '''
        self.util = Filerepe2e_Util()

        tinctest.logger.info("[STLRTest] Running resume_faults")   

        if not trans_state == 'failover_to_mirror' :
            tinctest.logger.info("[STLRTest] fault for %s resume" % fault_name)
            (ok1,out1) = self.util.inject_fault(f=fault_name, m = 'async', y = 'resume', r = 'primary', H ='ALL')
            if not ok1:
                raise Exception("[STLRTest]Fault resume failed")   
            tinctest.logger.info("[STLRTest]Done fault for %s resume" % fault_name)

        if trans_state == 'postmaster_reset':
            (ok1,out1) = self.util.inject_fault(f=fault_name, m = 'async', y = 'resume', r = 'mirror', H ='ALL')
            if not ok1:
                tinctest.logger.info("[STLRTest]Failed fault for %s resume on mirror" % fault_name)

        if trans_state == 'failover_to_primary' :
            self.check_fault_status(fault_name,'completed')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

    def checkPSQLRun(self, test):
        '''Check if the psql run started in parallel is over before running the _post.sql '''
        tinctest.logger.info("[STLRTest] Running checkPSQLRun")   
        cmd_str = 'ps -ef|grep sub_transaction_limit_removal|grep psql'
        while(1):
            is_running = 0 
            (rc , out) = shell.run(cmd_str)
            for line in out:
                if '%s' %test in line:
                    is_running = 1 
            if is_running == 0:
                return True
            else:
                sleep(10)
        return False
        

    def resume_filerep_resync(self):
        self.util = Filerepe2e_Util()

        tinctest.logger.info("[STLRTest] Running resume_filerep_resync")   

        tinctest.logger.info("[STLRTest] fault for failover_to_mirror resume")
        (ok1,out1) = self.util.inject_fault(f='filerep_resync', m = 'async', y = 'resume', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done fault for failover_to_mirror resume")
        sleep(10)

    def stop_start_validate(self, expect_down_segments=False):
        """
        Do gpstop -i, gpstart and see if all segments come back up fine 
        """        
        self.gpstart = GpStart()
        self.gpstop = GpStop()
        tinctest.logger.info("[STLRTest] Running stop_start_validate")   

        tinctest.logger.info("[STLRTest]Shutting down the cluster")
        ok = self.gpstop.run_gpstop_cmd(immediate = 'i')
        if not expect_down_segments:
            if not ok:
                raise Exception('[STLRTest]Problem while shutting down the cluster')
            tinctest.logger.info("[STLRTest]Successfully shutdown the cluster.")

        tinctest.logger.info("[STLRTest]Restarting the cluster.")
        ok = self.gpstart.run_gpstart_cmd()
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        if not ok:
            raise Exception('[STLRTest]Failed to bring the cluster back up')
        tinctest.logger.info("[STLRTest]Successfully restarted the cluster.")
        if not self.anydownsegments():
           raise Exception("[STLRTest]segments were marked down")
        else:
           return (True, "All segments are up")

    def run_gprecoverseg(self,recover_option):
        '''
        @summary : Call gpecoverseg full or incremental to bring back the cluster to sync
        '''
        self.gpr = GpRecover()

        tinctest.logger.info("[STLRTest] Running run_gprecoverseg")   
        tinctest.logger.info("[STLRTest] START printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        if recover_option == 'full':
            self.gpr.full()
        else:
            self.gpr.incremental()
        #Wait till the primary and mirror are in sync
        tinctest.logger.info("[STLRTest] Middle printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        self.gpr.wait_till_insync_transition()
        tinctest.logger.info("[STLRTest] END printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        
    def run_restart_database(self):
        '''
        @summary : Restart the database
        '''
        self.gpstart = GpStart()
        self.gpstop = GpStop()
        tinctest.logger.info("[STLRTest] Running run_restart_database")   
        ok = self.gpstop.run_gpstop_cmd(immediate = 'i')
        tinctest.logger.info(ok)
        ok = self.gpstart.run_gpstart_cmd()
        tinctest.logger.info(ok)       
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        
       
    def reset_faults(self,fault_name,current_cluster_state):
        ''''
        @summary : Reset the faults at the end of test 
        '''
        self.util = Filerepe2e_Util()
        tinctest.logger.info("[STLRTest] Running reset_faults")   

        tinctest.logger.info("[STLRTest] Resetting fault before ending test")

        (ok1,out1) = self.util.inject_fault(f=fault_name, m = 'async', y = 'reset', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done resetting %s fault" %(fault_name))

        if current_cluster_state == 'resync':
            tinctest.logger.info("[STLRTest] printing gp segment configuration")
            (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
            tinctest.logger.info(gp_seg_conf)

            (ok1,out1) = self.util.inject_fault(f='filerep_resync', m = 'async', y = 'reset', r = 'primary', H ='ALL')
            if not ok1:
                raise Exception("[STLRTest]Fault injection failed")   
            tinctest.logger.info("[STLRTest]Done filerep_resync fault")

        (ok1,out1) = self.util.inject_fault(f='checkpoint', m = 'async', y = 'reset', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done resetting checkpoint fault" )
        
    def do_gpcheckcat(self, dbname=None, alldb=False, online=False, outputFile='checkcat.out', outdir=None):
        self.dbstate = DbStateClass('run_validation')
        tinctest.logger.info("[STLRTest] Running do_gpcheckcat")
        self.dbstate.check_catalog()
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        return True

    def _validation(self):
        '''
        @summary :gpcheckcat and gpcheckmirrorintegrity
        '''
        
        ###psql.run_shell_command("CHECKPOINT; CHECKPOINT; CHECKPOINT;CHECKPOINT; CHECKPOINT;")
        ###sleep(30) # sleep for some time for the segments to be in sync before validation
 
        self.dbstate = DbStateClass('run_validation')
        tinctest.logger.info("[STLRTest] Running _validation")

        outfile = local_path("subt_checkcat.out")
        self.dbstate.check_catalog(outputFile=outfile)
         
        self.dbstate.check_mirrorintegrity()

    def inject_and_resume_fault(self, fault_name, trans_state):
        self.check_fault_status(fault_name, 'triggered')
        self.filerep_fault(trans_state)
        if trans_state == 'failover_to_mirror' :
            PSQL.run_sql_file(local_path('test_while_ct.sql'))
        self.resume_faults(fault_name, trans_state)
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)


    def run_post_sqls(self, fault_name ='', trans_state=''):
        checkDBUp()
        if (trans_state == 'failover_to_primary' or trans_state == ''):   
            post_sql = "failover_sql/subt_create_table_ao_post_commit"
        else:
            post_sql = "failover_sql/subt_create_table_ao_post_abort"       
            
        sql_file = post_sql+".sql"
        ans_file = post_sql+".ans"
        out_file = post_sql+".out"

        PSQL.run_sql_file(sql_file = local_path(sql_file), out_file = local_path(out_file))
        diff_res = Gpdiff.are_files_equal(local_path(out_file), local_path(ans_file))
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        
        if not diff_res:
           self.fail("[STLRTest]Gpdiff failed for : %s %s" %(fault_name, trans_state))


    def reset_all_faults(self):
        ''''
        @summary : Reset all faults on primary and mirror 
        '''
        tinctest.logger.info("[STLRTest] Running reset_all_faults")   
        self.util = Filerepe2e_Util()

        (ok1,out1) = self.util.inject_fault(f='all', m = 'async', y = 'reset', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done resetting all faults on primary")

        (ok1,out1) = self.util.inject_fault(f='all', m = 'async', y = 'reset', r = 'mirror', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)
        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")   
        tinctest.logger.info("[STLRTest]Done resetting all faults fault on mirror") 

    def kill_zombies(self):
        ''' 
        @summary : There are stray zombie processes running after each test. This method clears them 
        '''
        tinctest.logger.info("[STLRTest] Running kill_zombies")
        cmd_str = "ps -ef | grep \"port\" | awk '{print $3 \"#\" $2}' | grep -w 1"
        cmd = Command("shell_command", cmd_str)
        tinctest.logger.info('Executing command: %s : %s' %("shell command", cmd_str))
        cmd.run()
        result = cmd.get_results()
        out = result.stdout
        lines = out.split('\n')
        for line in lines:
            pids = line.split('#')
            if pids[0] == '1':
               kill_str= "kill -9 %s" %(pids[1])
               cmd2 = Command("kill_command", kill_str)
               cmd2.run()


    def skip_checkpoint(self):
        ''' 
        @summary : Routine to inject fault that skips checkpointing 
        '''

        self.util = Filerepe2e_Util()

        tinctest.logger.info("[STLRTest] Running skip_checkpoint")

        (ok1,out1) = self.util.inject_fault(f='checkpoint', m = 'async', y = 'reset', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")
        tinctest.logger.info("[STLRTest]Done resetting the checkpoint fault")

        (ok1,out1) = self.util.inject_fault(f='checkpoint', m = 'async', y = 'skip', r = 'primary', H ='ALL')
        tinctest.logger.info("[STLRTest] printing gp segment configuration")
        (gp_seg_conf) = PSQL.run_sql_command("select * from gp_segment_configuration order by dbid")
        tinctest.logger.info(gp_seg_conf)

        if not ok1:
            raise Exception("[STLRTest]Fault injection failed")
        tinctest.logger.info("[STLRTest]Done skipping the checkpoint fault")



    def method_setup(self):
        tinctest.logger.info("Performing setup tasks")
        gpfs=Gpfilespace()
        gpfs.create_filespace('subt_filespace_a')

    def cleandb(self):
        db = Database()
        db.setupDatabase('gptest')
