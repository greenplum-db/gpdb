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
import time

import tinctest
from gppylib.db import dbconn
from tinctest.lib import local_path
from gppylib.commands.base import Command, REMOTE
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.lib import SmartCmd
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify


class GpUtility(object):

    def __init__(self):
        self.gphome = os.environ.get('GPHOME')
        self.pgport = os.environ.get('PGPORT')
        self.master_dd = os.environ.get('MASTER_DATA_DIRECTORY')

    def run(self, command=''):
        '''Runs command, returns status code and stdout '''
        cmd = Command(name='Running cmd %s'%command, cmdStr="source %s/greenplum_path.sh; %s" % (self.gphome,command))
        tinctest.logger.info(" %s" % cmd)
        try:
            cmd.run(validateAfter=False)
        except Exception, e:
            tinctest.logger.error("Error running command %s\n" % e)
        result = cmd.get_results()
        tinctest.logger.info('Command returning status code: %s'%result.rc)
        tinctest.logger.info('Command returning standard output: %s'%result.stdout)
        return (result.rc, result.stdout)


    def run_remote(self, standbyhost, rmt_cmd, pgport = '', standbydd = ''):
        '''Runs remote command and returns rc, result '''
        export_cmd = "source %s/greenplum_path.sh;export PGPORT=%s;export MASTER_DATA_DIRECTORY=%s" % (self.gphome, pgport, standbydd) 
        remote_cmd = "gpssh -h %s -e '%s; %s'" % (standbyhost, export_cmd, rmt_cmd)
        tinctest.logger.info('cmd is %s'%remote_cmd)
        cmd = Command(name='Running Remote command', cmdStr='%s' % remote_cmd)
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        return result.rc,result.stdout


    def check_and_start_gpdb(self):
        if not self.gpstartCheck():
            self.run('gpstart -a')
        else:
            return True

    def check_and_stop_gpdb(self):
        if self.gpstartCheck():     
           self.run('gpstop -a')
        else:
           return True      

    def gpstartCheck(self):
        bashCmd = 'source ' + (self.gphome)+'/greenplum_path.sh;'+(self.gphome)+'/bin/pg_ctl status -D $MASTER_DATA_DIRECTORY | grep \'pg_ctl: server is running\''
        (ok,out) = self.run(bashCmd)
        if out == '' or out == []:
            return False
        return True


    def get_pid_by_keyword(self, host="localhost", user=os.environ.get('USER'), pgport=os.environ.get('PGPORT'), keyword='None', option=''):
        """
        tag
        """
        # On some agents the value of the MASTER_DATA_DIRECTORY i.e keyword here
        # contains // in the string which causes the grep to fail in finding the process pid
        # so replacing // with /
        keyword = keyword.replace("//","/")
        grepCmd="ps -ef|grep postgres|grep %s|grep \'%s\'|grep \'%s\'|grep -v grep|awk \'{print $2}\'" % (pgport, keyword, option)
        cmd = "%s/bin/gpssh -h %s -u %s \"%s\"" % (os.environ.get('GPHOME'), host, user, grepCmd)
        (rc, out) = self.run(cmd)
        output = out.split()
        if not rc and len(output) > 1:
            return output[2]
        else:
            tinctest.logger.error("error: process %s does not exist on host %s"%(keyword,host))
            return -1 


    def gpstart_and_verify(self, master_dd=os.environ.get('MASTER_DATA_DIRECTORY'), host='localhost'):
        pid = walrepl.get_postmaster_pid(master_dd, host)
        tinctest.logger.info("gpstart_verify: master dd is %s, host is %s, master pid is %s"%(master_dd,host, pid))
        if pid == -1:
            return False
        cmd_str = 'kill -s 0 {pid}'.format(pid=pid)
        cmd = SmartCmd(cmd_str, host=host)
        cmd.run()
        results = cmd.get_results()
        return results.rc == 0

    def gpstop_and_verify(self, option = ''):
        """
        stop, and verify all the cluster completely shutdown
        """      
        sql = 'select hostname, port, content, preferred_role from gp_segment_configuration;'
        segments = self.run_SQLQuery(sql,dbname= 'template1')
        if option == '-u':
            std_sql = 'drop user if exists gptest; create user gptest LOGIN PASSWORD \'testpassword\'; '
            PSQL.run_sql_command(std_sql, flags = '-q -t', dbname= 'template1')
            new_entry = 'local all gptest password \n'
            self.add_entry(line = new_entry)
            self.run('gpstop -a %s'%option)
            return self.gpreload_verify('gptest', 'testpassword')    
        (rc, output) = self.run('gpstop -a %s'%option)
        if rc != 0:
            return False
        if option == '-r':
            no_sync_up_segment_sql = 'select content, preferred_role from gp_segment_configuration where mode <> \'s\' or status <> \'u\';'
            segments_with_issue = self.run_SQLQuery(no_sync_up_segment_sql,dbname= 'template1')
            if segments_with_issue:
                return False
        elif option == '-y':
            for segment in segments:
                seg_content = segment[2]
                seg_preferred_role = segment[3]
                seg_host = segment[0]
                seg_port = segment[1]
                if seg_content == -1 and seg_preferred_role == 'm':
                    pid = self.get_pid_by_keyword(host=seg_host, pgport=seg_port, keyword='bin')
                    if  pid < 0:
                        tinctest.logger.error("standby host should not be shutdown.")
                else:
                    pid = self.get_pid_by_keyword(host=seg_host, pgport=seg_port, keyword='bin')  
                    if pid > 0:
                        tinctest.logger.error("%s segment on host %s was not properly shutdown"%(seg_preferred_role, seg_host))
                        return False              
        elif option == '-m':
            for segment in segments:
                seg_content = segment[2]
                seg_preferred_role = segment[3]
                seg_host = segment[0]
                seg_port = segment[1]
                if seg_content == -1 and seg_preferred_role == 'p':
                    pid = self.get_pid_by_keyword(host=seg_host, pgport=seg_port, keyword='bin')  
                    if pid > 0:
                        tinctest.logger.error("master should shutdown but still is running")
                        return False    
                else:
                    pid = self.get_pid_by_keyword(host=seg_host, pgport=seg_port, keyword='bin')
                    if pid < 0:
                        tinctest.logger.error("%s segment on host %s should not be shutdown"%(seg_preferred_role, seg_host))
                        return False  
        else:
            for segment in segments:
                seg_content = segment[2]
                seg_preferred_role = segment[3]
                seg_host = segment[0]
                seg_port = segment[1]
                pid = self.get_pid_by_keyword(host=seg_host, pgport=seg_port, keyword='bin')
                if pid > 0:
                    tinctest.logger.error("%s segment on host %s was not properly shutdown"%(seg_preferred_role, seg_host))
                    return False
        return True 


    def check_standby_presence(self):
        get_stdby = 'select hostname from gp_segment_configuration where content = -1 and preferred_role = \'m\';'
        host = PSQL.run_sql_command(get_stdby, flags='-q -t', dbname='template1')
        if host.strip():
            return True
        else:
            return False
             

    def add_entry(self, line = ''):
        ''' add one entry into the pg_hba.conf'''
        with open(local_path(self.master_dd + '/pg_hba.conf'),'a') as pgfile:
            pgfile.write(line) 

    def gpreload_verify(self, user, password):
        '''verify that the entry is effective after reload '''
        cmd = 'export PGPASSWORD=%s; psql -U %s -d template1'%(password, user)
        (rc, out) = self.run(command = cmd)
        if "FATAL" in out or rc != 0:
            return False
        else:
            return True


    def install_standby(self, new_stdby_host = '', standby_mdd=os.environ.get('MASTER_DATA_DIRECTORY')): 
        """
        Creating a new standby, either on localhost, or on a remote host
        failback: if set to be true, means that installs a standby master to the old master's dir, later to be activated.
        standby_mdd: by default, will be under $MASTER_DATA_DIRECTORY/newstandby, but can be set to other dir.
        new_stdby_host: if empty, will automatically use the first primary segment node as default standby host.
        """
        if new_stdby_host == '':
            get_host = 'select hostname from gp_segment_configuration where content = 0 and preferred_role = \'p\';'
            host = PSQL.run_sql_command(get_host, flags = '-q -t', dbname= 'template1')
            new_stdby_host = host.strip()
            tinctest.logger.info("New standby host to config %s" % new_stdby_host)
            if not new_stdby_host:
                raise Exception("Did not get a valid host name to install standby") 
        standbyport = str(int(self.pgport) + 1)
        (standby_filespace,filespace_loc) = self.get_concatinate_filespace(standby_mdd)
        for fsp in filespace_loc:
            self.create_dir(new_stdby_host,fsp)
            self.clean_dir(new_stdby_host, fsp)
        init_stdby = "gpinitstandby -a -s %s -P %s -F \'%s\'"%(new_stdby_host,
                                                               standbyport,
                                                               standby_filespace)    
        tinctest.logger.info("New standby init command: %s" % init_stdby)
        self.run(init_stdby)    
    
    def get_concatinate_filespace(self, standby_mdd=os.environ.get('MASTER_DATA_DIRECTORY')):
        '''
        Get a concatinated filespace string, to create standby on a host with dir equal to: filespace_dir/newstandby
        '''
        standby_dir = os.path.split(standby_mdd)[0]
        last_dir = os.path.split(standby_dir)[1]
        pg_system_filespace = os.path.join(os.path.split(standby_dir)[0], last_dir + '_newstandby')        

        pg_filespace = 'pg_system:'+ pg_system_filespace
        fs_sql = '''select fselocation from pg_filespace_entry
                    where fsefsoid != 3052 and fsedbid = (select dbid from gp_segment_configuration
                    where role = 'p' and content = -1);'''
        result = PSQL.run_sql_command(fs_sql, flags = '-q -t', dbname= 'template1')
        result = result.strip()
        filespace_loc = result.split('\n')
        tinctest.logger.info('filespace_loc: %s'%filespace_loc)
        fsp_dir = []
        fsp_dir.append(pg_system_filespace)
        for fse_loc in filespace_loc:
            fse_loc = fse_loc.strip()
            if fse_loc != '':
                # get leafe node from the dir
                prune_last1 = os.path.split(fse_loc)[0]
                # get the second level node name from backward
                parent_dir = os.path.split(prune_last1)[1]
                # remove the second level and continue to get the third level leafe node which is the filespace name
                prune_last2 = os.path.split(prune_last1)[0]
                filespace = os.path.split(prune_last2)[1]
                # specify the filespace location on standby side
                filespace_location_stdby = os.path.split(standby_dir)[0] + '_newstandby' + '/'+filespace+'/'+parent_dir + '/' + last_dir + '_newstandby'
                pg_filespace = pg_filespace + ',' + filespace + ':' + filespace_location_stdby
                fsp_dir.append(filespace_location_stdby)
        tinctest.logger.info("pg_filespace is %s"%pg_filespace)    
        tinctest.logger.info("fsp_dir is %s"%fsp_dir)
        return (pg_filespace,fsp_dir)


    def failback_to_original_master(self, old_mdd = os.environ.get('MASTER_DATA_DIRECTORY'),new_master_host = '', new_master_mdd=os.environ.get('MASTER_DATA_DIRECTORY'), new_master_port = ''): 
        """ 
        Creating a new standby, either on localhost, or on a remote host
        failback: if set to be true, means that installs a standby master to the old master's dir, later to be activated.
        standby_mdd: by default, will be under $MASTER_DATA_DIRECTORY/newstandby, but can be set to other dir.
        new_stdby_host: if empty, will automatically use the first primary segment node as default standby host.
        """
        (master_filespace,filespace_loc) = self.get_concatinate_filespace_new_master(old_mdd, new_master_host, new_master_mdd, new_master_port)
        tinctest.logger.info('master_filespace is %s'%master_filespace)
        tinctest.logger.info('filespace_loc %s'%filespace_loc)
        for fsp in filespace_loc:
            self.create_dir(socket.gethostname(),fsp)
            self.clean_dir(socket.gethostname(),fsp)
        init_stdby = "gpinitstandby -a -s %s -P %s -F \'%s\'"%(socket.gethostname(),
                                                               self.pgport,
                                                               master_filespace)    
        tinctest.logger.info("New standby init command: %s" % init_stdby)
        self.run_remote(new_master_host, init_stdby, new_master_port, new_master_mdd)            
        self.activate(old_mdd,new_master_host, new_master_mdd, new_master_port)



    def activate(self, old_mdd=os.environ.get('MASTER_DATA_DIRECTORY'), new_master_host='', new_master_mdd='', new_master_port=''):
        ''' Stop the remote master and activate new standby on old  master host'''

        self.run_remote(new_master_host, 'gpstop -aim', new_master_port, new_master_mdd)

        gpactivate_cmd = 'gpactivatestandby -a -d %s' %(old_mdd)
        (rc, result) = self.run_remote(socket.gethostname(),gpactivate_cmd,self.pgport,self.master_dd)
        tinctest.logger.info('Result without force option to activate standby %s'%result)
        if (rc != 0) and result.find('Force activation required') != -1:
            tinctest.logger.info('activating standby failed, try force activation...')
            gpactivate_cmd = 'gpactivatestandby -a -f -d %s' %(old_mdd)
            (rc, result) = self.run_remote(socket.gethostname(),gpactivate_cmd,self.pgport,self.master_dd)
            if (rc != 0):
                tinctest.logger.error('Force activating standby failed!')
                return False
        tinctest.logger.info('standby acvitated, host value %s' % socket.gethostname())
        return True 

    def get_concatinate_filespace_new_master(self, old_mdd, new_master_host='', new_master_mdd='', new_master_port=''):
        ''' 
        Get a concatinated filespace string, inorder to init a standby on the old master host 
        '''
        fs_sql = ''' select fselocation from pg_filespace_entry 
                     where fsefsoid != 3052 and fsedbid = (select dbid from gp_segment_configuration 
                     where role = 'p' and content = -1);'''
        file = '/tmp/filespace.sql'
        fp = open(file,'w')
        fp.write(fs_sql)
        fp.close()
        cmd = 'scp %s %s:/tmp'%(file, new_master_host)
        self.run(cmd)
        #new_master_postfix = os.path.split(new_master_mdd)[1]
        pg_system_filespace = old_mdd
        last_dir = os.path.split(old_mdd)[1]
        pg_filespace = 'pg_system:'+ pg_system_filespace
        get_filespace_cmd = 'psql --pset pager=off template1 -f %s'%file      
        tinctest.logger.info('remote command to run for getting filespace locations %s'%get_filespace_cmd)
        (rc, stdout) = self.run_remote(new_master_host, get_filespace_cmd, new_master_port, new_master_mdd)        
        tinctest.logger.info('stdout from remote executing sqls on new master is %s'%stdout)
        result = stdout.strip()
        filespace_loc = result.split('\n')
        for i in xrange(3):
            if not filespace_loc:
                break
            else:
                filespace_loc.pop(0)
        for i in xrange(2):
            if not filespace_loc:
                break
            else:
                filespace_loc.pop()
        tinctest.logger.info('after removing unncessary result from gpssh, filespace_loc is %s'%filespace_loc)
        # strip and split each line, to retrieve each filespace loc
        filespace_list = []
        for fse_loc in filespace_loc:
            fse_loc = fse_loc.strip().split(' ')[2]
            filespace_list.append(fse_loc)
        tinctest.logger.info('filespace_list: %s'%filespace_list)
        tinctest.logger.info('split and continue removing unncessary information from gpssh, filespace_list is %s'%filespace_list)
        fsp_dir = []
        fsp_dir.append(pg_system_filespace)
        for fse_loc in filespace_list:
            fse_loc = fse_loc.strip()
            if fse_loc != '': 
                # truncate the last two leafe nodes inorder to get the filespace name
                prune_last1 = os.path.split(fse_loc)[0]
                prune_last2 = os.path.split(prune_last1)[0]
                filespace = os.path.split(prune_last2)[1]
                
                filespace_location_stdby = prune_last1.replace('_newstandby', '') + '/' + last_dir
                #prune_last1 =  prune_last1.replace(new_master_postfix,'master')
                pg_filespace = pg_filespace + ',' + filespace + ':' + filespace_location_stdby
                fsp_dir.append(filespace_location_stdby)
        tinctest.logger.info("pg_filespace is %s"%pg_filespace)    
        tinctest.logger.info("fsp_dir is %s"%fsp_dir)
        return (pg_filespace,fsp_dir)


    def clean_dir(self,host,dir):
        cmd = "%s/bin/gpssh -h %s -e 'rm -rf %s'" % (self.gphome,host,dir)
        self.run(cmd)

    def create_dir(self, host, dir):
        cmd = "%s/bin/gpssh -h %s -e 'mkdir -p %s'" % (self.gphome,host,dir)
        self.run(cmd)

    def remove_standby(self):
        self.run('gpinitstandby -r -a')  

    def run_SQLQuery(self, exec_sql, dbname, hostname='localhost', port=os.environ['PGPORT']):
        with dbconn.connect(dbconn.DbURL(dbname=dbname, hostname=hostname, port=port)) as conn:
            curs = dbconn.execSQL(conn, exec_sql)
            results = curs.fetchall()
        return results   

    def stop_logger_process(self, host, port):
        grep_expr =  '[p]ostgres: port\s*%s,.*logger process' % port
        tinctest.logger.info('Grep expr = %s' % grep_expr)
        if host:
            cmd = Command('get the pid for a primary process', cmdStr='ps -ef | grep "%s" | awk \'{print \$2}\'' % grep_expr, 
                           ctxt=REMOTE, remoteHost=host)
        else:
            cmd = Command('get the pid for a primary process', cmdStr='ps -ef | grep "%s" | awk \'{print $2}\'' % grep_expr)

        cmd.run(validateAfter=True)
        result = cmd.get_results()
        tinctest.logger.info('Result of grep command = %s' % result)
        pid_list = map(int, result.stdout.strip().split('\n'))
        for pid in pid_list:
            tinctest.logger.info('Killing primary segment with pid %s' % pid)
            if host:
                cmd = Command('Stop a logger process', cmdStr='kill -SIGSTOP %s' % pid, ctxt=REMOTE, remoteHost=host)
            else:
                cmd = Command('Stop a logger process', cmdStr='kill -SIGSTOP %s' % pid, ctxt=REMOTE, remoteHost=host)
            cmd.run(validateAfter=True)

    def stop_postmaster_process(self, host, port):
        grep_expr =  '[p]ostgres -D .* -p %s' % port
        tinctest.logger.info('Grep expr = %s' % grep_expr)
        if host:
            cmd = Command('get the pid for a primary process', cmdStr='ps -ef | grep "%s" | awk \'{print \$2}\'' % grep_expr, 
                           ctxt=REMOTE, remoteHost=host)
        else:
            cmd = Command('get the pid for a primary process', cmdStr='ps -ef | grep "%s" | awk \'{print $2}\'' % grep_expr)

        cmd.run(validateAfter=True)
        result = cmd.get_results()
        tinctest.logger.info('Result of grep command = %s' % result)
        pid_list = map(int, result.stdout.strip().split('\n'))
        for pid in pid_list:
            tinctest.logger.info('Killing primary segment with pid %s' % pid)
            if host:
                cmd = Command('Stop a process on remote host', cmdStr='kill -SIGSTOP %s' % pid, ctxt=REMOTE, remoteHost=host)
            else:
                cmd = Command('Stop a process on remote host', cmdStr='kill -SIGSTOP %s' % pid)
            cmd.run(validateAfter=True)

    def stop_gpmmon_process(self, hostname):
        grep_expr = '[g]pmmon -D .*'
        tinctest.logger.info('Grep expr = %s' % grep_expr)
        cmd = Command('get the pid for gpmmon', cmdStr="ps -ef | grep '%s' | awk '{print \$2}'" % grep_expr, ctxt=REMOTE, remoteHost=hostname)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        pid = ' '.join([p.strip() for p in result.stdout.strip().split('\n')])
        if pid:
            cmd = Command('stopping the gpmmon process', cmdStr='kill -SIGSTOP %s' % pid, ctxt=REMOTE, remoteHost=hostname)
            cmd.run(validateAfter=True)
        else:
            raise Exception('Unable to find gpmmon process. Please make sure it is installed')

    def stop_gpsmon_process(self, hostname):
        grep_expr = '[g]psmon -m .*'
        tinctest.logger.info('Grep expr = %s on host %s' % (grep_expr, hostname))
        retries = 60
        for r in range(retries):
            cmd = Command('get the pid for gpsmon', cmdStr="ps -ef | grep '%s' | awk '{print \$2}'" % grep_expr, ctxt=REMOTE, remoteHost=hostname)
            cmd.run(validateAfter=True)
            result = cmd.get_results()
            if not result.stdout.strip() or len(result.stdout.strip().split('\n')) > 1:
                time.sleep(1)
                continue
            pid = ' '.join([p.strip() for p in result.stdout.strip().split('\n')]) 
            if pid:
                cmd = Command('stopping the gpsmon process', cmdStr='kill -SIGSTOP %s' % pid, ctxt=REMOTE, remoteHost=hostname)
                cmd.run(validateAfter=True)
                break
            else:
                raise Exception('Unable to find gpsmon process. Please make sure it is installed')

    def verify_gpmmon(self):
        gpmmon_grep_expr = '[g]pmmon -D .*'

        cmd = Command('get the pid for gpmmon', cmdStr="ps -ef | grep '%s' | awk '{print $2}'" % gpmmon_grep_expr)
        try:
            cmd.run(validateAfter=True)
        except Exception as e:
            pass
        else:
            result = cmd.get_results()
            if result.stdout.strip() != '':
                raise Exception('Found gpmmon process with pid %s' % result.stdout.strip())

    def verify_gpsmon(self, hostname):
        gpsmon_grep_expr = '[g]psmon -m .*'

        cmd = Command('get the pid for gpsmon process', cmdStr="ps -ef | grep '%s' | awk '{print \$2}'" % gpsmon_grep_expr,
                      ctxt=REMOTE, remoteHost=hostname)
        try:
            cmd.run(validateAfter=True)
        except Exception as e:
            pass
        else:
            result = cmd.get_results()
            if result.stdout.strip() != '':
                raise Exception('Found gpsmon process with pid %s' % result.stdout.strip())
