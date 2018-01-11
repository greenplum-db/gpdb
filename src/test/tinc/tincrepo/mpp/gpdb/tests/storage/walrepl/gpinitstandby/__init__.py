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
import pexpect as pexpect

import tinctest
from tinctest.lib import local_path
from gppylib.commands.base import Command
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify
from mpp.gpdb.tests.storage.walrepl.lib import WalReplException 
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility


class GpinitStandby(object):
    '''Class for gpinitstandby operations 
       Disclaimer: Some of these may repeat with the mpp/lib version'''
    def __init__(self):
        self.stdby = StandbyVerify()
        self.runmixin = StandbyRunMixin()
        self.runmixin.createdb(dbname='walrepl')        
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.config = GPDBConfig()
        self.pgutil = GpUtility()
        self.host = socket.gethostname()
 
    def run(self, option = ''):
        '''Runs gpinitstandby and returns True if successfull '''
        gpinitstandby_cmd = 'gpinitstandby -a %s' % option
        cmd = Command(name='Running Gpinitstandby', cmdStr="%s" % gpinitstandby_cmd)
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        if result.rc != 0:
            return False
        return True

    def verify_gpinitstandby(self, primary_pid):  
        '''Verify the presence of standby in recovery mode '''
        if (self.stdby.check_gp_segment_config()) and (self.stdby.check_pg_stat_replication()) and (self.stdby.check_standby_processes())and self.compare_primary_pid(primary_pid) :
            return True
        return False

    def get_masterhost(self):
        std_sql = "select hostname from gp_segment_configuration where content=-1 and role='p';"
        master_host = PSQL.run_sql_command(std_sql, flags = '-q -t', dbname= 'postgres')
        return master_host.strip()

    def get_standbyhost(self):
        std_sql = "select hostname from gp_segment_configuration where content='-1' and role='m';"
        standby_host = PSQL.run_sql_command(std_sql, flags = '-q -t', dbname= 'postgres')
        return standby_host.strip()

    def get_filespace_location(self):
        fs_sql = "select fselocation from pg_filespace_entry where fselocation like '%fs_walrepl_a%' and fsedbid=1;"
        filespace_loc = PSQL.run_sql_command(fs_sql, flags = '-q -t', dbname= 'postgres')
        return filespace_loc.strip()

    def get_standbyhostnode(self):
        '''
        Function used to obtain the hostname of one of the segment node inorder to use it as the standby master node" 
        @return : returns the hostname of the segment node which can be used as the standby master node
        '''
        hostlist = self.config.get_hosts()
        standby = ''
        for host in hostlist:
            if host.strip() != self.host:
                standby = host.strip()
        if len(standby) > 0 :
            return standby
        else:
            tinctest.logger.error('No segment host other than master available to have remote standby')

    def get_primary_pid(self):
        pid = self.pgutil.get_pid_by_keyword(pgport=os.environ.get('PGPORT'), keyword=self.mdd)
        if int(pid) == -1:
            raise WalReplException('Unable to get pid of primary master process')
        else:
            return int(pid)

    def compare_primary_pid(self, initial_pid):
        final_pid = self.get_primary_pid()
        if initial_pid == final_pid :
            return True
        return False

    def create_dir_on_standby(self, standby, location):
        fs_cmd = "gpssh -h %s -e 'rm -rf %s; mkdir -p %s' " % (standby, location, location)
        cmd = Command(name='Make dierctory on standby before running the command', cmdStr = fs_cmd)
        tinctest.logger.info('%s' % cmd)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            raise WalReplException('Unable to create directory on standby')
        else:
            return True
      
    def initstand_by_with_default(self):
        master_host = self.get_masterhost()
        gp_cmd =  "/bin/bash -c 'gpinitstandby -s %s'" % (master_host)
        logfile = open(local_path('install.log'),'w')

        child = pexpect.spawn(gp_cmd, timeout=400)
        child.logfile = logfile
        sleep(2)
        check = child.expect(['.* Enter standby filespace location for filespace pg_system .*', ' '])
        if check != 0:
            child.close()

        l_file = open(local_path('install.log'),'r')
        lines = l_file.readlines()
        for line in lines:
            if 'default: NA' in line:
                return True
        return False

    def init_with_prompt(self,filespace_loc):
        standby = self.get_standbyhostnode() 
        gp_cmd =  "/bin/bash -c 'gpinitstandby -s %s -a'" % (standby)
        logfile = open(local_path('install2.log'),'w')

        child = pexpect.spawn(gp_cmd, timeout=400)
        child.logfile = logfile
        sleep(5)
        check = child.expect(['.* Enter standby filespace location for filespace.*', ' '])
        child.sendline(filespace_loc)

        sleep(10)
        check = child.expect(['.*Successfully created standby master.*'])
        if check != 0:
            tinctest.logger.error('gpinitstandy failed')
            return False
        child.close()
        return True
