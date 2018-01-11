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
import shutil

import tinctest
from tinctest.lib import run_shell_command, local_path
from gppylib.commands.base import Command
from gppylib.commands import gp
from gppylib.gparray import GpArray
from gppylib.db import dbconn
from time import sleep

from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl.lib import WalReplException 
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

class GpactivateStandby(object):
    '''Class for gpactivatestandby operations '''
    
    standby_port = '5656'
    db_name = 'walrepl'

    def __init__(self):
        self.gpinit = GpinitStandby()
        self.pgutil = GpUtility()
        self.runmixin = StandbyRunMixin()
        self.runmixin.createdb(dbname='walrepl')
        self.gphome = os.environ.get('GPHOME')
        self.pgport = os.environ.get('PGPORT')
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.config = GPDBConfig()
        self.host = socket.gethostname()

        dburl = dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        self.numcontent = gparray.getNumSegmentContents()
        self.orig_master = gparray.master

    def run_remote(self, standbyhost, rmt_cmd, pgport = '', standbydd = ''):
        '''Runs remote command and returns rc, result '''
        export_cmd = "source %s/greenplum_path.sh;export PGPORT=%s;export MASTER_DATA_DIRECTORY=%s" % (self.gphome, pgport, standbydd) 
        remote_cmd = "gpssh -h %s -e '%s; %s'" % (standbyhost, export_cmd, rmt_cmd)
        cmd = Command(name='Running Remote command', cmdStr='%s' % remote_cmd)
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        return result.rc,result.stdout


    def activate(self, option=''):
        ''' Stop the master and activate current standby to master'''
        standby_host = self.get_current_standby() 
        standby_port = self.get_standby_port()
        standby_loc = self.get_standby_dd()

        self.run_remote(self.host, 'gpstop -aim', pgport=self.pgport, standbydd=self.mdd)

        gpactivate_cmd = 'gpactivatestandby -a -d %s %s' %(standby_loc, option)
        (rc, result) = self.run_remote(standby_host, gpactivate_cmd, pgport = standby_port, standbydd=standby_loc)
        tinctest.logger.info('Result without force option to activate standby %s'%result)
        if (rc != 0) and result.find('Force activation required') != -1:
            tinctest.logger.info('activating standby failed, try force activation...')
            gpactivate_cmd = 'gpactivatestandby -a -f -d %s %s' %(standby_loc, option)
            (rc, result) = self.run_remote(standby_host, gpactivate_cmd, pgport = standby_port, standbydd=standby_loc)
            if (rc != 0):
                tinctest.logger.error('Force activating standby failed!')
                return False
        tinctest.logger.info('standby acvitated, host value %s' % standby_host)
        return True 

    def remove_standby(self):
        return self.gpinit.run(option='-r')

    def failback_to_original_master(self):
        # Check if master is running.
        bashCmd = (self.gphome)+'/bin/pg_ctl status -D $MASTER_DATA_DIRECTORY | grep \'pg_ctl: server is running\''
        cmd = Command(name='Running cmd %s'%bashCmd, cmdStr="source %s/greenplum_path.sh; %s" % (self.gphome,bashCmd))
        try:
            cmd.run()
        except Exception, e:
            tinctest.logger.error("Error running command %s\n" % e)
            return
        
        result = cmd.get_results()
        out = result.stdout
        if not out:
            tinctest.logger.info('Start the old master again ...')
            master = gp.MasterStart("Starting orig Master", self.orig_master.datadir, self.orig_master.port, self.orig_master.dbid, 0, self.numcontent, None, None, None)
            master.run(validateAfter=True)
            result = master.get_results()
            tinctest.logger.info ('orig Master started result : %s' % result.stdout)
            if result.rc != 0:
                raise WalReplException('Unable to start original master process')
            Command('gpinitstandby -ra', 'gpinitstandby -ra').run()
            # failing back to old master, it takes a little bit to prepare the cluster ready for connection
            if os.path.exists(local_path('drop_filespace.sql')):
                PSQL.run_sql_file(local_path('drop_filespace.sql'), dbname=self.db_name)
 
    def verify_gpactivatestandby(self, primary_pid, host, port, mdd):  
        '''Verify that standby is now acting as master '''
        final_pid = self.get_standby_pid(host, port, mdd)
        if (self.compare_standby_pid(final_pid, primary_pid)) and (self.check_process_on_new_master(host, port)) and (self.select_from_table(host, port, mdd)) :
            return True
        return False 

    def get_current_standby(self):
        return self.config.get_master_standbyhost()

    def get_filespace_location(self):
        fs_sql = """select fselocation from pg_filespace_entry
            where fselocation like '%fs_walrepl_a%' and
                fsedbid = (select dbid from gp_segment_configuration
                            where role = 'p' and content = -1);"""
        filespace_loc = PSQL.run_sql_command(fs_sql, flags = '-q -t', dbname= 'postgres')
        return filespace_loc.strip()

    
    def get_standby_port(self):
        stdby_port_sql = 'select port from gp_segment_configuration where content = -1 and preferred_role=\'m\' ;'
        result = PSQL.run_sql_command(stdby_port_sql, flags = '-q -t', dbname= 'template1')
        standby_port = result.strip()
        if len(standby_port) > 0:
            return standby_port
        else:
            tinctest.logger.error('Not able to get the standby port, check if standby configured!')


    def get_standby_dd(self):
        stdby_dir_sql = 'select fselocation from gp_segment_configuration, pg_filespace_entry where dbid = fsedbid and fsefsoid=3052 and content = -1 and preferred_role=\'m\' ;'
        result = PSQL.run_sql_command(stdby_dir_sql, flags = '-q -t', dbname= 'template1')
        standby_loc = result.strip()
        if len(standby_loc) > 0:
           return standby_loc
        else:
            tinctest.logger.error('Not able to get the standby dir, check if standby configured!')

   

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

    def get_standby_pid(self, standby_host, standby_port, standby_mdd):
        '''
        Don't use member funciton to get standby hsot, port, and mdd, as the standby may have already
        been activated and old master has been stopped.
        '''
        pid = self.pgutil.get_pid_by_keyword(host=standby_host, pgport=standby_port, keyword=standby_mdd)
        if pid ==  -1:
            raise WalReplException('Unable to get pid of standby master process')
        else:
            return pid

    def compare_standby_pid(self, final_pid, initial_pid):
        tinctest.logger.info ('Initial pid = %s; Final pid = %s ' % (initial_pid, final_pid))
        if initial_pid == final_pid :
            return True
        return False
    
    def check_process_on_new_master(self, host, port):
        rmt_cmd = "gpssh -h %s -e 'ps -ef|grep %s | grep -v grep' "% (host, port)
        cmd = Command(name='Running a remote command', cmdStr = rmt_cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        tinctest.logger.info('%s\n%s' %(rmt_cmd, result.stdout))

        result = result.stdout.splitlines()
        if len(result) > 3 :
            for line in result:
                if 'wal receiver' in line:
                    return False
        else:
            return False
        tinctest.logger.info('Only master processes exists on standby ....\n')
        return True

    def select_from_table(self, host, port , mdd):
        psql_cmd = 'psql %s -q -t -c "select count(*) from gpact_t1;" ' % self.db_name
        (rc,result) = self.run_remote(host, psql_cmd, pgport= port, standbydd = mdd)
        if int(result.splitlines()[1].split()[1].strip()) == 19:
            tinctest.logger.info('The select query returned a value of 19')
            return True
        return False

