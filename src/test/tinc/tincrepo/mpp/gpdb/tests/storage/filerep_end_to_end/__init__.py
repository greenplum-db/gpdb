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

import datetime
import fileinput
import re
import time
import unittest2 as unittest
import socket
import os, string, sys

import tinctest
from gppylib.commands.base import Command, REMOTE
from tinctest.lib import local_path, Gpdiff, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.filerep_util import Filerepe2e_Util

from time import sleep
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop
from mpp.lib.gpConfig import GpConfig
from mpp.lib.config import GPDBConfig

import subprocess

class FilerepTestCase(MPPTestCase):

    def __init__(self, methodName):    
        self.pgport = os.environ.get('PGPORT')
        self.util = Filerepe2e_Util()
        self.gpconfig = GpConfig()
        self.config = GPDBConfig()
        self.gpr = GpRecover(self.config)
        self.dbstate = DbStateClass('run_validation',self.config)
        self.gpstart = GpStart()
        self.gpstop = GpStop()
        super(FilerepTestCase,self).__init__(methodName)

    def sleep(self, seconds=60):
        time.sleep(seconds)

    def create_file_in_datadir(self, content, role, filename):
        dbid = self.config.get_dbid(content=content, seg_role=role)
        host, datadir = self.config.get_host_and_datadir_of_segment(dbid=dbid)
        file_path = os.path.join(datadir, filename)
        cmd = Command('create a file', 'touch %s' % file_path, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)

    def remove_file_in_datadir(self, content, role, filename):
        dbid = self.config.get_dbid(content=content, seg_role=role)
        host, datadir = self.config.get_host_and_datadir_of_segment(dbid=dbid)
        file_path = os.path.join(datadir, filename)
        cmd = Command('remove a file', 'rm %s' % file_path, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)

    def get_timestamp_of_file_in_datadir(self, content, role, filename):
        dbid = self.config.get_dbid(content=content, seg_role=role)
        host, datadir = self.config.get_host_and_datadir_of_segment(dbid=dbid)
        file_path = os.path.join(datadir, filename)
        cmd = Command('check timestamp', """ python -c "import os; print os.stat('%s').st_mtime" """ %
                      file_path, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)
        res = cmd.get_results().stdout.strip()
        return res

    def verify_file_exists(self, content, role, filename):
        dbid = self.config.get_dbid(content=content, seg_role=role)
        host, datadir = self.config.get_host_and_datadir_of_segment(dbid=dbid)
        file_path = os.path.join(datadir, filename)
        cmd = Command('check if file exists', 'test -f %s' % file_path, ctxt=REMOTE, remoteHost=host)
        cmd.run(validateAfter=True)

    def handle_ext_cases(self,file):
        """
        @file: wet sql file to replace with specific machine env.
        """

        host = str(socket.gethostbyname(socket.gethostname())) #Must be an IP
        querystring = "gpfdist://"+host+":8088"
        
        if os.path.isfile(file):
            for line in fileinput.FileInput(file,inplace=1):
               line = re.sub('gpfdist.+8088',querystring,line)
               print str(re.sub('\n','',line))

    def handle_hybrid_part_cases(self, file):
        """
        @file: hybrid sql file to replace with specific machine env
        """

        querystring = "FROM '"+local_path('hybrid_part.data')+"'" 
        if os.path.isfile(file):
            for line in fileinput.FileInput(file,inplace=1):
                line = re.sub('FROM\s\'.+hybrid_part.data\'',querystring,line)
                print str(re.sub('\n','',line))


    def preprocess(self):
        """ 
        Replace the hard-coded information from sql files with correct hostname and ip address,etc 
        """

        list_workload_dir = ['set_sync1','sync1','set_ck_sync1','ck_sync1',
                        'set_ct','ct','set_resync','resync','set_sync2','sync2']
        for dir in list_workload_dir:
            sql_path = os.path.join(local_path(dir),'sql')
            ans_path = os.path.join(local_path(dir),'expected')
            for file in os.listdir(sql_path):
                    if (file.find('wet_ret')>=0):
                       self.handle_ext_cases(os.path.join(sql_path,file))
                    if (file.find('hybrid_part')>=0):
                       self.handle_hybrid_part_cases(os.path.join(sql_path,file))  
            for file in os.listdir(ans_path):
                    if (file.find('wet_ret')>=0):
                       self.handle_ext_cases(os.path.join(ans_path,file))
                    if (file.find('hybrid_part')>=0):
                       self.handle_hybrid_part_cases(os.path.join(ans_path,file)) 


    def clean_data(self):
        """ 
        Clean the data by removing the external table, otherwise, more data will be appended to the
        same external table from running multiple sql files. 
        """  

        test = local_path("")
        test = str(test) +"data/*.*"
    
        cmd = 'rm -rfv '+test
        run_shell_command(cmd)       

    def anydownsegments(self):
        """
        checks if any segments are down
        """        

        tinctest.logger.info("Checking if any segments are down")
        num_segments_down = self.count_of_nodes_down()
        if int(num_segments_down) == 0:
           return True
        else:
           return False

    def stop_start_validate(self, stopValidate=True):
        """
        Do gpstop -i, gpstart and see if all segments come back up fine 
        """        

        tinctest.logger.info("Performing stop start validate")
        tinctest.logger.info("Shutting down the cluster")
        ok = self.gpstop.run_gpstop_cmd(immediate = 'i', validate=stopValidate)
        if not ok and stopValidate:
           raise Exception('Problem while shutting down the cluster')
        tinctest.logger.info("Successfully shutdown the cluster.")

        tinctest.logger.info("Restarting the cluster.")
        ok = self.gpstart.run_gpstart_cmd()
        if not ok:
            raise Exception('Failed to bring the cluster back up')
        tinctest.logger.info("Successfully restarted the cluster.")
        if not self.anydownsegments():
           raise Exception("segments were marked down")
        else:
           return (True, "All segments are up")


    def method_reset_fault_injection(self):
        """
        Resets fault injection
        Return: (True, [result]) if OK, or (False, [result]) otherwise
        """        

        tinctest.logger.info("Resetting fault injection")
        
        (ok1,out1) = self.util.inject_fault(f='filerep_resync', m = 'async', y = 'reset', r = 'primary', H ='ALL')
        if not ok1:
            raise Exception("Fault injection failed")   
        tinctest.logger.info("Done Injecting Fault  to reset resync")

        return (True, str(out1))


    def method_resume_filerep_resync(self):
        """
        Resumes the process of resync
        """

        tinctest.logger.info("Resuming Resync")
        (ok, out) = self.util.inject_fault(f='filerep_resync', m='async',y='resume', r='primary', H='ALL')
        if not ok:
            raise Exception("Fault injection failed")   
        tinctest.logger.info("Done resuming resync")
        return (ok, out)

    def run_method_suspendresync(self):
        """
        Stops the cluster from going to resync
        """

        tinctest.logger.info("Suspending resync")
        (ok,out) = self.util.inject_fault(f='filerep_resync', m='async' , y='suspend', r ='primary', H='ALL')
        tinctest.logger.info('output from suspend resync %s'%out)
        if not ok:
            raise Exception("Fault injection failed")   
        tinctest.logger.info("Done Injecting Fault to suspend resync")
        return (ok, out)
      

    def count_of_masters(self):
        """
        Gives count of number of nodes in the cluster that are master 
        Return: count of number of nodes in the cluster that are master
        """

        tinctest.logger.info("Count the number of masters")
        cmd = "select count(*) from gp_segment_configuration where content = -1"
        (out) = PSQL.run_sql_command(cmd)
        num_master = out.split('\n')[3].strip()
        return num_master 


    def count_of_nodes(self):
        """
        Gives count of number of nodes in the cluster
        Return: count of number of nodes in the cluster
        """

        tinctest.logger.info("Counting number of nodes")
        cmd = "select count(*) from gp_segment_configuration"
        (num_cl) = PSQL.run_sql_command(cmd)
        total_num_rows = num_cl.split('\n')[3].strip()
        return total_num_rows


    def count_of_nodes_in_ct(self):
        """
        Gives count of number of nodes in change tracking
        Return: count of number of nodes in change tracking
        """

        tinctest.logger.info("Counting number of nodes in ct")
        sqlcmd = "select count(*) from gp_segment_configuration where mode = 'c'"
        (num_cl) = PSQL.run_sql_command(sqlcmd)
        num_cl = num_cl.split('\n')[3].strip()
        return num_cl


    def count_of_nodes_down(self):
        """
        Gives count of number of nodes marked as down
        Return: count of number of nodes marked as down
        """

        tinctest.logger.info("Counting the number of nodes down")
        sqlcmd = "select count(*) from gp_segment_configuration where status = 'd'"
        (num_down) = PSQL.run_sql_command(sqlcmd)
        num_down = num_down.split('\n')[3].strip()
        return num_down    


    def count_of_nodes_sync(self):
        """
        Gives count of number of nodes in sync
        Return: count of number of nodes in sync
        """

        tinctest.logger.info("Counting the number of nodes in sync")        
        sqlcmd = "select count(*) from gp_segment_configuration where mode = 's'"
        (num_sync) = PSQL.run_sql_command(sqlcmd)
        num_sync = num_sync.split('\n')[3].strip()
        return num_sync


    def count_of_nodes_not_sync(self):
        """
        Gives count of number of nodes not in sync
        Return: count of number of nodes not in sync
        """

        tinctest.logger.info("Counting number of nodes not in sync")
        sqlcmd = "select count(*) from gp_segment_configuration where mode <> 's'"
        (num_sync) = PSQL.run_sql_command(sqlcmd)
        num_sync = num_sync.split('\n')[3].strip()
        return num_sync

    def inject_fault_on_first_primary(self):
        """
	@product_version gpdb:[4.3.3.0-], gpdb:[4.2.8.1-4.2]
        """
        tinctest.logger.info("\n Injecting faults on first primary")
        (ok,out) = self.util.inject_fault(f='filerep_immediate_shutdown_request', m='async' , y='infinite_loop', r ='primary', seg_id=2, sleeptime=300)
        if not ok:
            raise Exception("Fault filerep_immediate_shutdown_request injection failed")   

        (ok,out) = self.util.inject_fault(f='fileRep_is_operation_completed', m='async' , y='infinite_loop', r ='primary', seg_id=2)
        if not ok:
            raise Exception("Fault fileRep_is_operation_completed injection failed")   
        tinctest.logger.info("\n Done Injecting Fault")


    def inject_fault_on_first_mirror(self):
        """
	@product_version gpdb:[4.3.3.0-], gpdb:[4.2.8.1-4.2]
        """
        sqlcmd = "select dbid from gp_segment_configuration where content=0 and role='m'"
        (first_mirror_dbid) = PSQL.run_sql_command(sqlcmd)
        first_mirror_dbid = first_mirror_dbid.split('\n')[3].strip()

        tinctest.logger.info("\n Injecting faults on first mirror")
        flag = self.util.check_fault_status(fault_name='fileRep_is_operation_completed', status='triggered', max_cycle=100);
        if not flag:
            raise Exception("Fault fileRep_is_operation_completed didn't trigger")   
 
        (ok,out) = self.util.inject_fault(f='filerep_consumer', m='async' , y='panic', r ='mirror', seg_id=first_mirror_dbid)
        if not ok:
            raise Exception("Fault filerep_consumer injection failed")   
        tinctest.logger.info("\n Done Injecting Fault")

    def setupGpfdist(self, port, path):
        gpfdist = Gpfdist(port , self.hostIP())
        gpfdist.killGpfdist()
        gpfdist.startGpfdist(' -t 30 -m 1048576 -d '+path)
        return True

    def cleanupGpfdist(self, port,path):
        gpfdist = Gpfdist(port , self.hostIP())
        gpfdist.killGpfdist()
        return True

    def hostIP(self):
        ok = run_shell_command('which gpfdist')
        if not ok:
            raise GPtestError("Error:'which gpfdist' command failed.")
        hostname = socket.gethostname()
        if hostname.find('mdw') > 0 :
            host = 'mdw'
        else:
            host = str(socket.gethostbyname(socket.gethostname())) #Must be an IP
        tinctest.logger.info('current host is %s'%host)
        return host

    def method_setup(self):
        tinctest.logger.info("Performing setup tasks")
        gpfs=Gpfilespace()
        gpfs.create_filespace('filerep_fs_a')
        gpfs.create_filespace('filerep_fs_b')
        gpfs.create_filespace('filerep_fs_c')
        gpfs.create_filespace('filerep_fs_z')
        gpfs.create_filespace('sync1_fs_1') 
 
        # Set max_resource_queues to 100 
        cmd = 'gpconfig -c max_resource_queues -v 100 '
        ok = run_shell_command(cmd)
        if not ok:
            raise Exception('Failure during setting the max_resource_queues value to 100 using gpconfig tool')
        #Restart the cluster
        self.gpstop.run_gpstop_cmd(immediate = 'i')
        ok = self.gpstart.run_gpstart_cmd()
        if not ok:
            raise Exception('Failure during restarting the cluster')
        return True


    def get_ext_table_query_from_gpstate(self):
        outfile = local_path("gpstate_tmp")
        ok = run_shell_command("gpstate --printSampleExternalTableSql >"+ outfile)
        querystring = ""
        flag = 'false'
        out = open(outfile, 'r').readlines()
        for line in out:
            line.strip()
            if (line.find('DROP EXTERNAL TABLE IF EXISTS gpstate_segment_status')>=0):
                flag = 'true'
            if flag == 'true':
                querystring = querystring + line
        return querystring ############RUN QYUERY

    def check_gpstate(self, type, phase):
        """ 
        Perform gpstate for each different transition state
        @type: failover type
        @phase: transition stage, can be sync1, ck_sync1, ct, resync, sync2
        """       

        if phase == 'sync1':
            state_num = self.query_select_count("select count(*) from gpstate_segment_status where role = preferred_role and mirror_status ='Synchronized' and status_in_config='Up' and instance_status='Up'")
            sync1_num = self.query_select_count("select count(*) from gp_segment_configuration where content <> -1")
            if int(sync1_num) <> int(state_num):
                raise Exception("gpstate in Sync state failed")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))

        elif phase == 'ct':
            p_num = self.query_select_count("select count(*) from gpstate_segment_status where role = preferred_role and mirror_status ='Change Tracking'  and role = 'Primary' and status_in_config='Up' and instance_status='Up'")
            m_num = self.query_select_count("select count(*) from gpstate_segment_status where role = preferred_role and mirror_status ='Out of Sync'  and role = 'Mirror' and status_in_config='Down' and instance_status='Down in configuration' ")

            if int(p_num) <> int(m_num):
                raise Exception("gpstate in CT state failed")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))

        elif phase == 'resync_incr':
            
            if type == 'primary':
                query = "select count(*) from gpstate_segment_status where role = preferred_role and mirror_status ='Resynchronizing' and  status_in_config='Up' and instance_status='Up'  and resync_mode= 'Incremental'"
                resync_incr_num = self.query_select_count(query)
            else:
                query = "select count(*) from gpstate_segment_status where mirror_status ='Resynchronizing' and  status_in_config='Up' and instance_status='Up' and resync_mode= 'Incremental'"
                resync_incr_num = self.query_select_count(query)
            
            query_num_rows = "select count(*) from gp_segment_configuration where content <> -1"
            num_rows = self.query_select_count(query_num_rows)
            
            if int(resync_incr_num) <> int(num_rows):
                tinctest.logger.info("resync_incr_num query run %s" % query)
                tinctest.logger.info("num_rows query run %s" % query_num_rows)
                raise Exception("gpstate in Resync Incremental  state failed. resync_incr_num %s <> num_rows %s" % (resync_incr_num, num_rows))
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))

        elif phase == 'resync_full':
            num_rows = self.query_select_count("select count(*) from gp_segment_configuration where content <> -1")
          
            if type == 'primary':
                resync_full_num = self.query_select_count("select count(*) from gpstate_segment_status where role = preferred_role and mirror_status ='Resynchronizing'  and  status_in_config='Up' and instance_status='Up'  and resync_mode= 'Full'")
            else:
                resync_full_num = self.query_select_count("select count(*) from gpstate_segment_status where mirror_status ='Resynchronizing'  and  status_in_config='Up' and instance_status='Up'  and resync_mode= 'Full'")

            if int(resync_full_num) <> int(num_rows):
                raise Exception("gptate in Resync Full state failed")
            tinctest.logger.info("Done Running gpstate in %s phase " %(phase))
        
        return True
    
    def trigger_transition(self):
        PSQL.run_sql_file(local_path('mirrors.sql'))
        

    def run_gpstate(self, type, phase):            
        """
        Perform gpstate for each different transition state
        @type: failover type
        @phase: transition stage, can be sync1, ck_sync1, ct, resync, sync2
        """

        tinctest.logger.info("running gpstate")
        querystring = self.get_ext_table_query_from_gpstate()
        file1 = local_path('create_table_gpstate.sql')
        f1 = open(file1,'w')
        f1.write(querystring)
        f1.write('\n')
        f1.close()
        PSQL.run_sql_file(local_path('create_table_gpstate.sql'))

        gpstate_outfile = local_path('gpstate_out')
        cmd = 'gpstate -s -a > %s 2>&1' % (gpstate_outfile)

        ok  = run_shell_command(cmd)
        self.check_gpstate(type, phase)
        return ok


    def check_mirror_seg(self, master=False):
        tinctest.logger.info("running check mirror")
        self.dbstate.check_mirrorintegrity()

    def do_gpcheckcat(self, dbname=None, alldb=False, online=False, outputFile='checkcat.out', outdir=None):
        tinctest.logger.info("running gpcheckcat")
        self.dbstate.check_catalog(outputFile=outputFile)

    def query_select_count(self,sqlcmd):
        (num) = PSQL.run_sql_command(sqlcmd)
        num = num.split('\n')[3].strip()
        return num
    
    def method_run_failover(self,type):
        """
        Inject fault to failover nodes
        @type: primary [induces fault in mirror] mirror [creates panic in primary]   
        Return: (True, [result of fault injection]) if OK, or (False, [result of fault injection]) otherwise
        """

        if type == 'primary':
            tinctest.logger.info("\n primary failover")
            (ok,out) = self.util.inject_fault(f='filerep_consumer', m='async' , y='fault', r ='mirror', H='ALL')
            tinctest.logger.info("\n Done Injecting Fault")

        elif type == 'mirror':
            tinctest.logger.info("\n Mirror failover")
            (ok,out) = self.util.inject_fault(f='postmaster', m='async' , y='panic', r ='primary', H='ALL')
            tinctest.logger.info("\n Done Injecting Fault")
        return True

    def wait_till_change_tracking_transition(self):
        self.util.wait_till_change_tracking_transition()

    def wait_till_insync_transition(self):
        self.gpr.wait_till_insync_transition()
   
    def run_gprecoverseg(self,recover_mode):
        if recover_mode == 'full':
            self.gpr.full()
        else:
            self.gpr.incremental()

    def run_gpconfig(self, parameter, master_value, segment_value):
        if (parameter is not None):
            self.gpconfig.setParameter(parameter, master_value, segment_value)
            self.gpstop.run_gpstop_cmd(restart='r')

    def inject_fault(self, fault = None, mode = None, operation = None, prim_mirr = None, host = 'All', table = None, database = None, seg_id = None, sleeptime = None, occurence = None):
        if (fault == None or mode == None or operation == None or prim_mirr == None):
            raise Exception('Incorrect parameters provided for inject fault')

        (ok,out) = self.util.inject_fault(f=fault, m=mode , y=operation, r=prim_mirr, H='ALL', table=table, database=database, sleeptime=sleeptime, o=occurence, seg_id=seg_id)

class Gpfdist:

    def __init__(self, port, hostname):
        "init"
        self.port = port
        self.hostname = hostname

    def startGpfdist(self, options):
        tinctest.logger.info("start hosting the data")
        p = subprocess.Popen(['ssh', self.hostname, 'source %s/greenplum_path.sh && gpfdist -p %s %s & ' % (os.environ['GPHOME'], self.port, options)])
        tinctest.logger.info("gpfdist cmd: gpfdist" + options) 
        if sys.platform.find("darwin") >= 0:
            sleep(10)
        else:
            sleep(5)
        return True

    def killGpfdist(self):
        tinctest.logger.info("kill the gpfdist process")
        cmd  = 'ps -ef|grep \"gpfdist\"|grep -v grep|awk \'{print $2}\''
        process = subprocess.Popen(cmd, shell=True,
                           stdout=subprocess.PIPE, 
                           stderr=subprocess.PIPE)
        out, err = process.communicate()
        pid_list = out.strip().split('\n')
        for pid in pid_list:
            self.killProcbyPid(pid.strip())
        return True

    def killProcbyPid(self, pid=None):
        if pid:
            result = run_shell_command('kill -9 %s > /dev/null'%pid)
            return result
        else:
            tinctest.logger.info("pid is None and not valid to kill")
