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
import random

from time import sleep
from random import randint
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command
from subprocess import Popen, PIPE
from mpp.lib.config import GPDBConfig

GPDBNAME = str(os.environ["PGDATABASE"])

'''
Creates Faults for the scenario
'''
class Fault(Command):
    def __init__(self, cmd = None):
        Command.__init__(self, 'Running fault command', cmd)

    def _run_sys_cmd(self, cmd_str, validate = False):
        '''helper function to run a sys cmd'''
        tinctest.logger.info("execute:" +cmd_str)
        cmd = Fault(cmd_str)
        cmd.run(validateAfter = validate)
        return cmd.get_results()
        
    def kill_segment_processes_randomly(self, role = 'm'):
        ''' Create system faults by killing segments randomly ''' 
        if role == 'm':
            Pattern    = 'con'
            segCntM = 2
        else :
            Pattern    = 'primary'
            segCntM = 1

        #1 get segment list
        SqlCmd = "select distinct port, hostname from gp_segment_configuration where preferred_role = '%s' and status = 'u' and content <> -1;" %role
        segInfo = PSQL.run_sql_command(SqlCmd).split('\n')
        segCnt = 1
        for info in segInfo[3::]:
            if not 'row' in info and not info is None and info <> '' and info <> '\n' and segCnt <= segCntM:
                #2. Sleep for random time
                if role == 'm':
                    ntime = randint(600, 900)
                else :
                    ntime = randint(900, 1800)
                tinctest.logger.info('sleep for %s sec. before killing next process' %ntime)
                sleep(ntime)
                (port, hostname) = info.split('|')
                sys_cmd = 'ssh %s ps aux | grep %s | grep %s  | awk \'{print $2}\' | xargs echo "kill -9 "' %(hostname, port, Pattern)
                pids = self._run_sys_cmd(sys_cmd)
                #3. Kill process
                sys_cmd2 = 'ssh %s %s' %(hostname, pids.stdout )
                result = self._run_sys_cmd(sys_cmd2)
            segCnt = segCnt + 1         


    def get_all_data_segments_hostname(self,role = 'm'):
        '''Gets all the hosts information based on the input role'''
        tinctest.logger.info('Getting all the hosts information..')
        get_hosts_cmd = "SELECT DISTINCT(hostname) FROM gp_segment_configuration WHERE role = '%s' AND content != -1" % (role)
        tinctest.logger.info(get_hosts_cmd)
        hosts = PSQL.run_sql_command(get_hosts_cmd).split('\n')[3:-3]
        return hosts
    
    def get_all_data_segments_port_nos(self,role = None):
        '''Gets all the port no. information based on the input role'''
        if role is None:
            whr_condition = ''
            tinctest.logger.info('Getting all the port no. informations for all data segments')
        else:
            whr_condition = "role = '%s' AND " % role
            tinctest.logger.info('Getting all the port no. informations for data segments with role \'%s\'' % role)
        get_ports_cmd = "SELECT DISTINCT(port) FROM gp_segment_configuration WHERE %s content != -1" % (whr_condition)
        tinctest.logger.info(get_ports_cmd)
        ports = PSQL.run_sql_command(get_ports_cmd).split('\n')[3:-3]
        return ports
        
    def get_host_port_mapping(self,role):
        """ 
        Returns a dictionary having key as hostname and value as a list of port nos.
        For e.g {'vm9':['22001','22000'] , 'vm10':{'42000','42001'}...}
        """        
        config = GPDBConfig()
        no_of_segments = config.get_countprimarysegments()
        hosts_dict = {}
        counter = 0
        while counter < no_of_segments:
            (host,port) = config.get_hostandport_of_segment(counter,role)
            if hosts_dict.has_key(host):
                hosts_dict[host].append(port)
            else:
                hosts_dict[host] = list()
                hosts_dict[host].append(port)
            counter += 1
        return hosts_dict

    def kill_processes_with_role(self,role = 'm'):
        '''kills all the data segment processes'''
        hosts_dict = self.get_host_port_mapping(role)
        # Kill all the postgres intsances of the concerned segment role running
        # on a particular host using the port nos.
        for host in hosts_dict:
            ports_list = hosts_dict[host]
            # Create a structure ('port1'|'port2'|...) from the list of ports
            # this is further passed as pattern to the grep expression.
            segment_ports = "("
            for port in ports_list[:-1]:
                segment_ports += port + "|" 
            segment_ports = segment_ports + ports_list[len(ports_list)-1] + ")"
            
            sys_cmd = "gpssh -h %s  -e ' ps aux | egrep '\\\''postgres -D.*-p %s'\\\'' | awk '\\\''{print \"kill -9 \"$2}'\\\'' | sh' "%(host,segment_ports)
            tinctest.logger.info("kill process command : %s"%sys_cmd)
            result = self._run_sys_cmd(sys_cmd)
    
    def kill_all_segment_processes(self,kill_all = True):
        '''Kills all/few the primary segment processess as per the flag kill_all'''
        # if kill_all is true, no need to find out specific segment processess
        if kill_all:
            grep_str = ''
        # else, find few specific segment process to kill
        else:
            port_nos = self.get_all_data_segments_port_nos('p')
            if len(port_nos) > 1:
                total_ports = len(port_nos)
                # selecting only half of the ports
                no_of_ports_to_select = total_ports / 2
                count = 0
                grep_str = "'"
                # selecting one port less,(eventually adding it after while loop) so as to 
                # remove the extra '|' symbol
                while(count < (no_of_ports_to_select-1) ):
                    index = random.randint(0,(total_ports - 1))
                    # pop removes the element itself from list
                    port = port_nos.pop(index)
                    # decreasing the size of list
                    total_ports = total_ports - 1 
                    grep_str += port.strip() + "|"
                    count += 1
                index = random.randint(0,(total_ports - 1))
                grep_str = "| grep " + grep_str + port_nos[index].strip() + "' "
            else:
                grep_str = '| grep %s ' %(port_nos[0].strip())
        
        all_hosts = self.get_all_data_segments_hostname('p')
        ssh_cmd = 'gpssh '
        host_names = ''
        
        for host in all_hosts:
            host_names += ' -h'+host
        
        ssh_cmd = ssh_cmd + host_names
        cmd_str = ssh_cmd + " -e ' ps aux | egrep con[0-9] %s| awk '\\\''{print \"kill -9 \"$2}'\\\'' | sh ' " % grep_str
        tinctest.logger.info(cmd_str)
        process = Popen(cmd_str,stdout=PIPE, shell=True)
        result = process.communicate()[0]
        return result
        
    def changetracking(self, type = 'mirror'):
        ''' Routine to inject fault that places system in change tracking'''
        tinctest.logger.info("Put system in changetracking ")
        if type == 'mirror':
            cmd_str = 'gpfaultinjector -f filerep_consumer -m async -y fault -r %s -H ALL' %type
        else :
            cmd_str = 'gpfaultinjector -f postmaster -m async -y fault -r primary -H ALL'

        result = self._run_sys_cmd("source $GPHOME/greenplum_path.sh; %s"%cmd_str, True)
        return result.stdout

    def is_changetracking(self):
        '''return true if system is in change tracking mode'''
        tinctest.logger.info("checking if database is in change tracking mode")
        cmd_str = "select 'down_segment' from gp_segment_configuration where mode = 'c'"
        out = PSQL.run_sql_command(cmd_str).count('down_segment') - 1
        if out > 0:
            return True
        return False
        
    def issue_checkpoint(self):
        ''' Forces the checkpoint to occur'''
        cmd_str = "CHECKPOINT;"
        result = PSQL.run_sql_command(cmd_str).split('\n')
        if len(result) > 1:
            return True
        else:
            return False

    def skip_checkpoint(self):
        ''' Routine to inject fault that skips checkpointing '''
        tinctest.logger.info("skipping checkpoint")
        cmd_str = 'source $GPHOME/greenplum_path.sh; gpfaultinjector -p %s -m async -H ALL -r primary -f checkpoint -y skip -o 0' % os.getenv('PGPORT')
        result = self._run_sys_cmd(cmd_str, True)
        return result.stdout

    def stop_db(self,options = None):
        ''' Stops the greenplum DB based on the options provided '''
        cmd_str = "source $GPHOME/greenplum_path.sh; gpstop -a"
        if options is None:
            options = ''
            
        cmd_str = cmd_str + options
        tinctest.logger.info("Starting the db operation: %s "%cmd_str)
        result = self._run_sys_cmd(cmd_str)
        return result
    
    def start_db(self,options = None):
        ''' Start the greenplum DB based on the options provided '''
        cmd_str = "source $GPHOME/greenplum_path.sh; gpstart -a"
        if options is None:
            options = ''

        cmd_str = cmd_str + options
        tinctest.logger.info("Starting the db operation: %s "%cmd_str)
        result = self._run_sys_cmd(cmd_str)
        return result
    
    def restart_db(self,options = 'ir'):
        ''' Restarts the greenplum DB '''
        return self.stop_db(options)

    def run_recovery(self,options = None):
        '''Runs the incremental recovery'''
        tinctest.logger.info('Invoking gprecoverseg to bring the segments up')
        if options is None:
            options = ''
        cmd_str = "source $GPHOME/greenplum_path.sh; gprecoverseg -a" + options 

        result = self._run_sys_cmd(cmd_str, False)
        return result.stdout

    def get_host_dir_oid(self):
        ''' Move database dir to some other location to take it offline '''
        # Get primary segment location
        cmd_str = "select s.hostname, fselocation from pg_filespace_entry f ,gp_segment_configuration s where s.dbid=f.fsedbid and  s.role='p' and content !='-1' order by s.dbid;"
        dirlist = PSQL.run_sql_command(cmd_str).split('\n')
        
        #select any segment randomly 
        segNo = 2 + randint( 1, 2) 
        (hostname, dirloc) = dirlist[segNo].split('|')
        
        #Find database oid
        cmd_str = "select oid from pg_database where datname = '%s';" %GPDBNAME
        oid = PSQL.run_sql_command(cmd_str).split('\n')[3].strip()

        return (hostname, dirloc, oid)

    def move_dir(self, hostname, fromDir, toDir):
        ''' Move Database dir on hostname fromDir toDir to take it offline '''
        tinctest.logger.info('Move dir ' + fromDir + ' To '+ toDir + ' On host :' + hostname)
        cmd = 'ssh %s mv %s %s' %(hostname, fromDir, toDir)
        self._run_sys_cmd(cmd)
        
    def drop_dir(self, hostname, DirPath):
        ''' Drop Database dir on hostname to take it offline '''
        tinctest.logger.info('Drop dir ' + DirPath + ' On host :' + hostname)
        cmd = 'ssh %s rm -rf %s' %(hostname, DirPath)
        self._run_sys_cmd(cmd)

    def drop_db(self, dbname = GPDBNAME):
        ''' Drop database '''
        tinctest.logger.info('Drop database ' + dbname)
        cmd = 'drop_db '+ dbname
        result = self._run_sys_cmd(cmd)
        tinctest.logger.info(result.stderr) 

    def create_db(self, dbname = GPDBNAME):
        ''' Create Database '''
        tinctest.logger.info('Create database ' + dbname)
        cmd = 'createdb '+ dbname
        result = self._run_sys_cmd(cmd)
        tinctest.logger.info(result.stderr)

    def rebalance_cluster(self):
        config = GPDBConfig()
        self.run_recovery('r')
        rtrycnt = 0
        while ((config.is_not_insync_segments()) == False and rtrycnt <= 5):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            sleep(10)
            rtrycnt = rtrycnt + 1
        #Many time it has been observed that gprecoverseg -ar marks segment down
        if config.is_not_insync_segments():
            return True
        else:
            self.run_recovery()
            rtrycnt = 0
            max_rtrycnt = 10
            while ((config.is_not_insync_segments()) == False and rtrycnt < max_rtrycnt):
                tinctest.logger.info("waiting [%s] for DB to recover" %rtrycnt)
                sleep(10)
                rtrycnt = rtrycnt + 1
            if rtrycnt < max_rtrycnt:
                return True
            else:
                tinctest.logger.error("Segments not up after incremental recovery!!")
                return False
    
