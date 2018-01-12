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
import tinctest
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL        
from mpp.lib.config import GPDBConfig
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.lib import Disk
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify

class OODClass(MPPTestCase):
    

    def __init__(self,methodName):
        self.gp = GpactivateStandby()
        self.verify = StandbyVerify()
        self.config = GPDBConfig()
        self.disk = Disk()
        self.sdby_mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.pgport = os.environ.get('PGPORT')
        super(OODClass,self).__init__(methodName)

    def initiate_standby(self):
        self.gp.create_standby(local='no')


    def check_standby(self):
        self.assertFalse(self.verify.check_standby_processes())

    def get_standby_dbid(self):
       std_sql = "select dbid from gp_segment_configuration where content='-1' and role='m';"
       standby_dbid = PSQL.run_sql_command(std_sql, flags = '-q -t', dbname= 'template1')
       return standby_dbid.strip()

    def restart_standby(self):
        sdby_host =  self.config.get_master_standbyhost()
        stdby_dbid = self.get_standby_dbid()
        cmd="pg_ctl -D %s -o '-p %s --gp_dbid=%s --gp_num_contents_in_cluster=2 --silent-mode=true -i --gp_contentid=-1 -E' start &"%(self.sdby_mdd, self.pgport, stdby_dbid)
        self.assertTrue(self.gp.run_remote(sdby_host,cmd, self.pgport, self.sdby_mdd))
        self.assertTrue(self.verify.check_standby_processes())

    def check_diskusage(self, host):  # This now checks for only /data
        (rc, result) = self.disk.get_disk_usage(host, '/data')
        if rc != 0:
            raise Exception ("The specified mount /data is not present for the device")
        else:
            available_usage = result
            return available_usage

    def _fill(self, filename, host):
        cmd_prefix = "ssh " +host+ " \""
        cmd_postfix = "\""
        location = '/data'
        if not os.path.isdir('%s/diskfill/' % location):
            os.makedirs('%s/diskfill/' % location)
        cmd_str = cmd_prefix + "dd if=/dev/zero bs=16384K count=2000 of=" +location+ "/diskfill/" + filename +cmd_postfix
        cmd = Command(name='Fill Disk', cmdStr=cmd_str)
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        if result.rc !=0:
            tinctest.logger.error('disk fill not working. Its already full')

    
    def filldisk(self):
        host =  self.config.get_master_standbyhost()
        disk_usage = self.check_diskusage(host)
        i = 0
        while(int(disk_usage.strip()) >1000000):
            filename = 'new_space_%s' % i
            self._fill(filename, host)
            i +=1
            disk_usage = self.check_diskusage(host)

    def remove_fillfiles(self, filename, host):
        location = '/data'
        cmd_str = "ssh %s rm %s/diskfill/%s*" % (host,location, filename)
        cmd = Command(name='Remove fill files', cmdStr=cmd_str)
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        if result.rc !=0:
            raise Exception('Unable to delete the fill files')
        return

        
    def cleanup(self):
        host =  self.config.get_master_standbyhost()
        self.remove_fillfiles('new_space', host)    
        #Recover segemnts in case segments and standby were on the same host
        cmd = Command(name='gprecoverseg', cmdStr='gprecoverseg -a')
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        if result.rc !=0:
            raise Exception('gprecoverseg failed')
        while(self.config.is_not_insync_segments() == False):
                tinctest.logger.info('Waiting for DB to be in sync')    
