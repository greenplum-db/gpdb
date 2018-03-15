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
import sys 
import time    
import fnmatch    
import socket


import tinctest
from gppylib.db import dbconn
from time import sleep
from gppylib.commands.base import Command, REMOTE
from tinctest.case import TINCTestCase
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass

class WalReplKillProcessTestCase(TINCTestCase):
  
   # this is not hard code, will be updated  
   stdby_host = 'localhost'
   stdby_port = '5432'
 
   def __init__(self,methodName):
       self.gphome = os.environ.get('GPHOME')
       self.pgport = os.environ.get('PGPORT')
       self.pgdatabase = os.environ.get('PGDATABASE')
       self.stdby_host = 'localhost'
       self.master_dd = os.environ.get('MASTER_DATA_DIRECTORY')
       self.pgutil = GpUtility()
       self.stdby = StandbyVerify()
       super(WalReplKillProcessTestCase,self).__init__(methodName)


   def killProcess_byPid(self, signal=9, pid_toKill=[], host="localhost"):
       pid_list = ""
       for pid in pid_toKill:
           pid_list = pid_list + " " + str(pid)

       kill_cmd = "%s/bin/gpssh -h %s -e 'kill -%s  %s'" % (os.environ.get('GPHOME'), host, signal, pid_list)
       (rc, result) = self.pgutil.run(kill_cmd)
       if rc == 0:
           tinctest.logger.info("Process killed, %s" % result)
           return True
       else:
           tinctest.logger.error("Killing process error, Status Code non zero, cmd: %s\n"%kill_cmd)
           return False

   def kill_walstartup(self):
       gpstdby = GpinitStandby()
       stdby_host = gpstdby.get_standbyhost()
       activate_stdby = GpactivateStandby()
       stdby_port = activate_stdby.get_standby_port()
       pid_list = []
       startup_pid = self.pgutil.get_pid_by_keyword(host=stdby_host, pgport=stdby_port, keyword="startup process")
       if int(startup_pid) == -1:
           tinctest.logger.error("error:startup process does not exist!")
           return False
       else:
           pid_list.append(startup_pid)
           self.killProcess_byPid(pid_toKill=pid_list, host=stdby_host)

   def kill_walreceiver(self):
       gpstdby = GpinitStandby()
       stdby_host = gpstdby.get_standbyhost()
       activate_stdby = GpactivateStandby()
       stdby_port = activate_stdby.get_standby_port()
       pid_list = []
       walreceiver_pid = self.pgutil.get_pid_by_keyword(host=stdby_host, pgport=stdby_port, keyword="wal receiver process")
       if int(walreceiver_pid) == -1:
           tinctest.logger.error("error: wal receiver process does not exist!")
           return False
       else:  
           pid_list.append(walreceiver_pid)
           self.killProcess_byPid(pid_toKill=pid_list, host=stdby_host)


   def kill_walsender_check_postmaster_reset(self):
       pid_list = []
       walsender_old_pid=self.pgutil.get_pid_by_keyword(pgport=self.pgport,keyword="wal sender process")
       if int(walsender_old_pid) == -1:
           tinctest.logger.error("error: process wal sender does not exist on host")
           return False
       else:
           pid_list.append(walsender_old_pid)
           self.killProcess_byPid(pid_toKill=pid_list)
       sleep(2)
       walsender_new_pid=self.pgutil.get_pid_by_keyword(pgport=self.pgport,keyword="wal sender process")
       if walsender_old_pid == walsender_new_pid:
          raise Exception("Killing walsender failed to force postmaster reset")
       else:
          return True         

   def kill_transc_backend_check_reset(self):
       dict_process = { 'stats collector process': -1, 'writer process': -1,
                       'checkpointer process': -1,'seqserver process': -1,
                       'ftsprobe process': -1,'sweeper process': -1,'wal sender process': -1}
       for process in dict_process:
           pid = self.pgutil.get_pid_by_keyword(pgport=self.pgport,keyword=process)
           dict_process[process] = pid 
       self.kill_transc_backend()
       for process in dict_process:
           pid = self.pgutil.get_pid_by_keyword(pgport=self.pgport,keyword=process)
           delay = 1
           while dict_process.get(process) == pid and delay < 5:
                pid = self.pgutil.get_pid_by_keyword(pgport=self.pgport,keyword=process)
                sleep(1)
                delay = delay +1
           if delay == 5:
                tinctest.logger.error("Killing transaction backend process failed to force postmaster reset: %s"%process)
                raise Exception("Killing transaction backend process failed to force postmaster reset child process") 
 
   def kill_transc_backend(self):
       pid_list = []
       sql = "SELECT procpid FROM pg_stat_activity WHERE datname='{0}' AND current_query like 'INSERT INTO%'".format(self.pgdatabase)    
       tinctest.logger.info("running sql command to get transaction backend process: ---  %s"%sql)
       procid = PSQL.run_sql_command(sql, flags = '-q -t', dbname= self.pgdatabase)
       count = 1
       while not procid.strip() and  count < 5:
             sleep(1)
             count += 1
             procid = PSQL.run_sql_command(sql, flags = '-q -t', dbname= self.pgdatabase)
       if procid.strip():
             tinctest.logger.info("got procid to kill: %s " % procid)
             pid_list.append(procid)
             self.killProcess_byPid(pid_toKill = pid_list)
       else:
             tinctest.logger.error("There is no active backend process")   

    
   def check_stdby_stop(self):
       gpstdby = GpinitStandby()
       stdby_host = gpstdby.get_standbyhost()
       activate_stdby = GpactivateStandby()
       stdby_port = activate_stdby.get_standby_port()     
       master_pid = self.pgutil.get_pid_by_keyword(host=stdby_host, pgport=stdby_port, keyword="master", option = "bin")
       if int(master_pid) != -1:
           raise Exception("standby should stop but failed!")        


   def start_stdby(self):
       gpstdby = GpinitStandby()
       stdby_host = gpstdby.get_standbyhost()
       stdby_dbid = self.get_standby_dbid()
       activate_stdby = GpactivateStandby()
       stdby_mdd = activate_stdby.get_standby_dd()
       stdby_port = activate_stdby.get_standby_port()
       cmd="pg_ctl -D %s -o '-p %s --gp_dbid=%s --gp_num_contents_in_cluster=2 --silent-mode=true -i --gp_contentid=-1 -E' start &"%(stdby_mdd, stdby_port, stdby_dbid)
       self.run_remote(stdby_host,cmd,stdby_port,stdby_mdd)
       

   def run_remote(self, standbyhost, rmt_cmd, pgport = '', standbydd = ''):
       '''Runs remote command and returns rc, result '''
       export_cmd = "source %s/greenplum_path.sh;export PGPORT=%s;export MASTER_DATA_DIRECTORY=%s" % (self.gphome, pgport, standbydd) 
       remote_cmd = "gpssh -h %s -e \"%s; %s\"" % (standbyhost, export_cmd, rmt_cmd)
       cmd = Command(name='Running Remote command', cmdStr='%s' % remote_cmd)
       tinctest.logger.info(" %s" % cmd)
       cmd.run(validateAfter=False)
       result = cmd.get_results()
       return result.rc,result.stdout


   def check_mirror_seg(self):
       dbstate = DbStateClass('run_validation')
       dbstate.check_mirrorintegrity(master=True)


   def get_standby_dbid(self):
       std_sql = "select dbid from gp_segment_configuration where content='-1' and role='m';"
       standby_dbid = PSQL.run_sql_command(std_sql, flags = '-q -t', dbname= 'template1')
       return standby_dbid.strip()


   def run_transaction_backend(self):
       tinctest.logger.info("local path for backend.sql is %s"%local_path(''))
       for file in os.listdir(local_path('')):
           if fnmatch.fnmatch(file,'backend.sql'):
                PSQL.run_sql_file(local_path(file))
                

   def get_down_segment(self):
       query = 'select * from gp_segment_configuration where mode <> \'s\' and status <>\'u\''
       result = PSQL.run_sql_command(query, flags = '-q -t', dbname='template1')
       return result.strip()


   def check_gpdb_status(self):
       down_segments = self.get_down_segment()
       self.assertEqual(down_segments,'')


   def gpstart_helper(self):
       '''helper method to run in scenario test'''
       (rc, result) = self.pgutil.run('gpstart -a')
       self.assertIn(rc,(0,1))

   def gpstop_helper(self):
       '''helper method to run in scenario test'''
       cmd = Command('run gpstop', cmdStr = 'gpstop -a')
       cmd.run(validateAfter=True)

   def gpinitstandby_helper(self):
       '''helper method to create a new standby'''
       self.pgutil.install_standby()
 
   def removestandby_helper(self):
       ''' helper method to remove standby'''
       self.pgutil.remove_standby()

   def verify_standby_sync(self):
       if (self.stdby.check_gp_segment_config()) and (self.stdby.check_pg_stat_replication()) and (self.stdby.check_standby_processes()):   
           return True
       else:
           raise Exception('standby and master out of sync!')

   def kill_standby_postmaster(self):
       pid_list = []
       delay = 0
       postmaster_pid = self.pgutil.get_pid_by_keyword(host=WalReplKillProcessTestCase.stdby_host, pgport=WalReplKillProcessTestCase.stdby_port, keyword="master", option="bin")
       while int(postmaster_pid) == -1 and delay < 20:
           sleep(1)
           delay = delay + 1
           postmaster_pid = self.pgutil.get_pid_by_keyword(host=WalReplKillProcessTestCase.stdby_host, pgport=WalReplKillProcessTestCase.stdby_port, keyword="master", option="bin")
       if int(postmaster_pid) == -1 or delay == 20:
           tinctest.logger.error("error: standby postmaster process does not exist!")
           return False
       else:
           pid_list.append(postmaster_pid)
           return self.killProcess_byPid(pid_toKill=pid_list, host=WalReplKillProcessTestCase.stdby_host)                                                                                            

   def initial_setup(self):
       keyword = 'rh55-qavm65'
       config = GPDBConfig()
       (seg_host,seg_port) = config.get_hostandport_of_segment(psegmentNumber = 0, pRole = 'p')
       cur_path = local_path('')
       dir1 = os.path.join(cur_path, 'dml', 'sql','insert_from_external.sql.in')
       dir2 = os.path.join(cur_path, 'dml', 'sql','insert_from_external.sql')
       dir3 = os.path.join(cur_path, 'dml', 'expected','insert_from_external.ans.in')
       dir4 = os.path.join(cur_path, 'dml', 'expected','insert_from_external.ans')

       f1 = open(dir1,'r')
       f2 = open(dir2,'w')
       f3 = open(dir3,'r')
       f4 = open(dir4,'w')
       for line in f1:
           f2.write(line.replace(keyword,seg_host))
       f1.close()
       f2.close()

       for line in f3:
           f4.write(line.replace(keyword,seg_host))
       f3.close()
       f4.close()

       dir5 = os.path.join(cur_path, 'dml', 'sql','insert_with_gpload.sql.in')
       dir6 = os.path.join(cur_path, 'dml', 'sql','insert_with_gpload.sql')
       yaml_path = local_path('dml/sql/config/gpl.yaml')
       f5 = open(dir5,'r')
       f6 = open(dir6,'w')
       for line in f5:
           f6.write(line.replace('gpl.yaml',yaml_path))
       f5.close()
       f6.close()

       dir7 = os.path.join(cur_path,'dml','sql','config','gpl.yaml.in')
       dir8 = os.path.join(cur_path,'dml','sql','config','gpl.yaml')
       f7 = open(dir7,'r')
       f8 = open(dir8,'w')
       for line in f7:
           if 'DATABASE' in line:
               f8.write(line.replace('tangp3',os.environ.get('PGDATABASE')))
           elif 'USER' in line:
               f8.write(line.replace('tangp3',os.environ.get('USER')))
           elif 'HOST' in line:
               f8.write(line.replace('rh55-qavm61',socket.gethostname()))
           elif 'PORT' in line and '5432' in line:
               f8.write(line.replace('5432',os.environ.get('PGPORT')))
           elif 'mydata' in line:
               f8.write(line.replace('mydata',local_path('dml/sql/gpload/mydata')))
           else:
               f8.write(line)
       f7.close()
       f8.close()

       dir9 = os.path.join(cur_path,'dml','expected','insert_with_gpload.ans.in')
       dir10 = os.path.join(cur_path,'dml','expected','insert_with_gpload.ans')
       f9 = open(dir9,'r')
       f10 = open(dir10,'w')
       for line in f9:
           f10.write(line.replace('gpl.yaml',yaml_path))
       f9.close()
       f10.close()

       dir11 = os.path.join(cur_path,'dml','sql','select_from_copy_table.sql.in')
       dir12 = os.path.join(cur_path,'dml','sql','select_from_copy_table.sql')
       f11 = open(dir11,'r')
       f12 = open(dir12,'w')
       for line in f11:
           if 'tenk.data' in line:
               f12.write(line.replace('tenk.data',local_path('dml/sql/_data/tenk.data')))
           else:
               f12.write(line)
       f11.close()
       f12.close()

       dir13 = os.path.join(cur_path,'dml','expected','select_from_copy_table.ans.in')
       dir14 = os.path.join(cur_path,'dml','expected','select_from_copy_table.ans')
       f13 = open(dir13,'r')
       f14 = open(dir14,'w')
       for line in f13:
           if 'tenk.data' in line:
               f14.write(line.replace('tenk.data',local_path('dml/sql/_data/tenk.data')))
           else:
               f14.write(line)
       f13.close()
       f14.close()


       external_table = local_path('dml/sql/_data/quote.csv')
       clean_file = 'rm -rf /tmp/quote.csv'
       rmt_cmd = "gpssh -h %s -e '%s' " % (seg_host, clean_file)
       cmd = Command(name='Running a remote command', cmdStr = rmt_cmd)
       cmd.run(validateAfter=False)
       command = 'scp %s %s:/tmp'%(external_table,seg_host)
       cmd = Command(name='run %s'%command, cmdStr = '%s' % command)
       try:
           cmd.run(validateAfter=True)
       except Exception, e:
           tinctest.logger.error("Error running command %s\n" % e)
