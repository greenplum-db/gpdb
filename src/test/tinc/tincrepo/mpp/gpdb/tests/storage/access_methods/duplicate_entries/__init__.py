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

@todo add kill process functions into common library

"""
############################################################################
# Env setup, follow the steps:
# 1. source the greenplum_path.sh,
# 2. source the tinc_env.sh from the tinc main branch
# 3. source the tincrepo_env.sh from the tincrepo private branch
#   where the tinc.py will drive running you testing case

############################################################################


import os  
import sys 
import time    
    
import tinctest
from gppylib.db import dbconn
from gppylib.commands.base import Command, REMOTE
from tinctest.case import TINCTestCase
from tinctest.lib import Gpdiff, local_path
from mpp.lib.PSQL import PSQL
from mpp.lib.gprecoverseg import GpRecover

class DuplicateEntriesTestCase(TINCTestCase):
 
    def __init__(self, methodName):
        self.test_method = methodName
        self.gphome = os.environ.get('GPHOME')
        self.pgport = os.environ.get('PGPORT')
        self.dbname = os.environ.get('PGDATABASE')
        self.master_dd = os.environ.get('MASTER_DATA_DIRECTORY')
        super(DuplicateEntriesTestCase,self).__init__(methodName)
    

    def killProcess_postmaster(self):
        """
        fault inject one primary segments.
        """

        primary_segments = self.get_segments_byrole('p')
        if not primary_segments:
           raise Exception("no primary segments.")

        mirror_segments = self.get_segments_byrole('m')
        if not mirror_segments:
            grep_string = 'mirrorless'
        else:
           grep_string = 'quiescent' #select for one primary segment to kill
        
        first_primary = primary_segments[0] # select one primary segment to do fault injector
        first_primary_host = first_primary[0]
        first_primary_port = first_primary[1]
        cmd="ps -ef|grep postgres|grep %s|grep %s|grep -v grep|awk \'{print $2}\'" % (first_primary_port,grep_string) 
        (statusCode,validPid) = self.verifyProcess(cmd,first_primary_host)
        if validPid == -1: 
            tinctest.logger.error("error: killing postmaster process on segment \"%s\" at port %s, process does not exist." % (first_primary_host,first_primary_port))
        else:
           self.killProcess_byPid(validPid, first_primary_host)



    def killProcess_byPid(self, pid, host="localhost", user=os.environ.get('USER')):
        kill_cmd = "%s/bin/gpssh -h %s -u %s -e 'kill -9  %s'" % (os.environ.get('GPHOME'), host, user, pid)
        (rc, result) = self.run_command(kill_cmd)
        if rc == 0:
            self.create_table()
        else:
            tinctest.logger.error("Killing process error, Status Code non zero, cmd: %s\n"%kill_cmd)

   
    def run_command(self, command, expected_rc = 0):
        cmd = Command(name='run %s'%command, cmdStr = 'source %s/greenplum_path.sh;%s' % (self.gphome, command))
        try:
            cmd.run(validateAfter=True)
        except Exception, e:
            tinctest.logger.error("Error running command %s\n" % e)

        result = cmd.get_results()
        if result.rc != expected_rc:
            tinctest.logger.error(" %s failed with error %s" % (cmd.cmdStr , result.stderr))
        return result.rc, result.stdout


    def verifyProcess(self,command, host="localhost", user=os.environ.get('USER')):
        """
        to verify if a process exist before killing it, if true, return valid process id and status code,
        else, return status code and a false pid(-1)
        """

        cmd = "%s/bin/gpssh -h %s -u %s \"%s\"" % (os.environ.get('GPHOME'), host, user, command)
        (rc, out) = self.run_command(cmd)
        output = out.split()
        if not rc and len(output) > 1:
            return (rc, output[2])
        else:
            return (rc, -1)


    def get_segments_byrole(self, role='p', dbname='template1'):
        get_seg_sql = "select distinct hostname,port from gp_segment_configuration where role = \'%s\' and content != -1;" % role
        result=PSQL.run_sql_command(get_seg_sql, flags = '-q -t', dbname=dbname)
        return result.strip().split('|')

    def create_table(self):
        tinctest.logger.info("Trigger transition")
        create_table_sql = "drop table if exists mpp19038; create table mpp19038(id serial, bar varchar);"
        PSQL.run_sql_command(create_table_sql, flags = '-q -t', dbname=self.dbname)


    def run_recover_rebalance(self):
        tinctest.logger.info("Run incremental recovery")
        Command('run gprecoverseg','gprecoverseg -a').run()
        self.check_insync_transition()
        tinctest.logger.info("Restarting cluster")
        Command('gpstop -ar','gpstop -ar').run()


    def check_insync_transition(self, dbname='template1'):
        """ 
        confirming that the current mode is in sync before performing the gpcheckmirrorseg, 
        resyncInterval increase 10 seconds for each new query, maximumly sleep 75 sec, can be tuned.
        """
        recoverseg = GpRecover()
        is_synchronized = recoverseg.wait_till_insync_transition()
        if not is_synchronized:
            self.fail('Segments are not in sync')    
   
    def reindex_vacuum(self):
        PSQL.run_sql_file(sql_file=local_path('reindex_vacuum.sql'),
                          out_file=local_path('reindex_vacuum.out')) 
 
    def check_duplicate_entry(self):     
        ans_file = local_path('check_duplicate_entry.ans')
        out_file=local_path('check_duplicate_entry.out')
        sql_file=local_path('check_duplicate_entry.sql')
        PSQL.run_sql_file(sql_file=sql_file,
                          out_file=out_file)        
        assert Gpdiff.are_files_equal(out_file,
                                      ans_file)
