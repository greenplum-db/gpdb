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
import unittest2 as unittest

import tinctest
from gppylib.commands.base import Command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

class GpstateTestCase(MPPTestCase):
   '''testcase for gpstart''' 
   
   origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY')

   def __init__(self,methodName):
       self.gputil = GpUtility()
       self.mirrorConfig = []
       self.master_port = os.environ.get('PGPORT')
       self.masterdd = os.environ.get('MASTER_DATA_DIRECTORY')
       self.activatestdby = ""
       super(GpstateTestCase,self).__init__(methodName)
    
   def setUp(self):
       self.gputil.check_and_start_gpdb()     
       stdby_presence = self.gputil.check_standby_presence()
       if stdby_presence:
           self.gputil.remove_standby()
       self.gputil.install_standby()
       get_mirror_sql = '''select port, hostname, fselocation
                      from gp_segment_configuration, pg_filespace_entry 
                      where dbid = fsedbid and content != -1 and preferred_role=\'m\' ;'''
       segments=self.gputil.run_SQLQuery(get_mirror_sql, dbname='template1')  
       for seg in segments:
           port = seg[0]
           host = seg[1]
           dir = seg[2]
           self.mirrorConfig.append(port)
           self.mirrorConfig.append(host)
           self.mirrorConfig.append(dir)

       self.activatestdby = GpactivateStandby()
           
   def tearDown(self):
       del self.mirrorConfig[:]
       self.gputil.remove_standby()

   def test_gpstate_disp_recovery(self):
       ''' run gpstate with -f option'''       
       standby_host = self.activatestdby.get_current_standby()
       standby_port =  self.activatestdby.get_standby_port()
       standby_dir = self.activatestdby.get_standby_dd()
       standby_pid = self.gputil.get_pid_by_keyword(host=standby_host, user=os.environ.get('USER'), 
                                                   pgport=standby_port,keyword='master',option='bin')
       (rc, stdout) = self.gputil.run('gpstate -f')
       self.assertEqual(rc, 0)
       context = stdout.split('\n')
       for line in context:
           if "=" not in line:
               continue
           items = line.split('=')
           if "Standby address" in line:  
               stdby_addr = items[1].strip()             
               self.assertEqual(stdby_addr, standby_host)
           elif "Standby data directory" in line:
               stdby_dir = items[1].strip()
               self.assertEqual(stdby_dir, standby_dir)
           elif "Standby port" in line:
               stdby_port = int(items[1].strip())
               self.assertEqual(stdby_port, int(standby_port))
           elif "Standby PID" in line:
               pid = items[1].strip()
               self.assertEqual(pid, standby_pid)       
         
            
   def test_gpstate_disp_failover(self):
       '''test if the master configuration detail changed after activating standby'''
       standby_host = self.activatestdby.get_current_standby()
       standby_port = self.activatestdby.get_standby_port()
       standby_dir = self.activatestdby.get_standby_dd()
       standby_pid = self.gputil.get_pid_by_keyword(host=standby_host, user=os.environ.get('USER'), 
                                                   pgport=standby_port,keyword='master',option='bin')
       self.activatestdby.activate()
       (rc,stdout)=self.activatestdby.run_remote(standby_host, rmt_cmd='gpstate -s',
                                                 pgport=standby_port,
                                                 standbydd=standby_dir)
       self.assertIn(rc, (0,1))
       context = stdout.split('\n')
       for line in context:
           if "=" not in line:
               continue
           items = line.strip().split('=')
           if "Master host" in line:
               master_host = items[1].strip()               
               self.assertEqual(master_host, standby_host)
           elif "Master postgres process ID" in line:
               master_pid = items[1].strip()
               self.assertEqual(master_pid, standby_pid)
           elif "Master data directory" in line:
               master_dir = items[1].strip()
               self.assertEqual(master_dir, standby_dir)
           elif "Master port" in line:
               master_port = int(items[1].strip())
               self.assertEqual(master_port, int(standby_port))
       self.gputil.failback_to_original_master(self.origin_mdd,standby_host,standby_dir,standby_port)
 

   def test_gpstate_active_segment_failover(self):
       ''' test if gpstate show correct # of up and down nodes after failover'''
       count_up_seg = '''select count(*) from gp_segment_configuration 
                      where content != -1 and status = \'u\';'''   
       count_down_seg = '''select count(*) from gp_segment_configuration 
                      where content != -1 and status = \'d\';''' 
       number_up_segment = PSQL.run_sql_command(count_up_seg, flags = '-q -t', dbname='template1')   
       number_down_segment = PSQL.run_sql_command(count_down_seg, flags = '-q -t', dbname='template1') 
       standby_host = self.activatestdby.get_current_standby()
       standby_port =  self.activatestdby.get_standby_port()
       standby_dir = self.activatestdby.get_standby_dd()
       self.activatestdby.activate() 
       (rc,stdout)=self.activatestdby.run_remote(standby_host, rmt_cmd='gpstate -Q',
                                                 pgport=standby_port,
                                                 standbydd=standby_dir)
       self.assertIn(rc, (0,1))
       context = stdout.split('\n')
       for line in context:
           if "=" not in line:
               continue
           items = line.strip().split('=')
           if "up segments" in line:
               self.assertEqual(number_up_segment.strip(),items[1].strip())
           elif "down segments" in line:
               self.assertEqual(number_down_segment.strip(),items[1].strip())
       self.gputil.failback_to_original_master(self.origin_mdd,standby_host,standby_dir,standby_port)
      
   
   def test_gpstate_disp_mirror_failover(self):
       ''' check if new master is able to get correct mirror configuration with gpstate -m'''
       inside_block = False 
       keywords = ("Mirror","Datadir","Port")
       standby_host = self.activatestdby.get_current_standby()
       standby_port =  self.activatestdby.get_standby_port()
       standby_dir = self.activatestdby.get_standby_dd()
       self.activatestdby.activate()    
       (rc,stdout)=self.activatestdby.run_remote(standby_host, rmt_cmd='gpstate -m',
                                                 pgport=standby_port,
                                                 standbydd=standby_dir) 
       self.assertEqual(rc, 0) 
       for line in stdout:
           if inside_block:
               line_split = line.split('')
               line_split = [elem for elem in line_split if elem != '']
               mirror_host = line_split[2]
               mirror_dir = line_split[3]
               mirror_port = line_split[4]
               self.assertTrue(mirror_host in self.mirrorConfig)
               self.assertTrue(mirror_dir in self.mirrorConfig)                      
               self.assertTrue(mirror_port in self.mirrorConfig)
           elif not all (s in line for s in keywords):
               continue
           else:
               inside_block = True
       self.gputil.failback_to_original_master(self.origin_mdd,standby_host,standby_dir,standby_port)
