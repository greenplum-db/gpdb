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
from time import sleep

import tinctest
from tinctest.lib import local_path

from mpp.lib.PSQL import PSQL
from tinctest.lib import run_shell_command

from mpp.gpdb.tests.storage.lib import Disk
class Steps():

    def __init__(self):
        self.disk = Disk()

    def get_segments(self): 
        get_seg_sql = "select distinct(hostname) from gp_segment_configuration;"
        res = PSQL.run_sql_command(get_seg_sql, dbname = 'template1', flags ='-q -t')
        return res.strip().split('\n')


    def checkDiskUsage(self, host, disk):  # This now checks for only /data
        (rc, result) = self.disk.get_disk_usage(host, '/'+disk)
        if rc != 0:
            raise Exception ("The specified mount /data is not present for the device")
        else:
            available_usage = result
            return available_usage

    def fillDisk(self, filename, host):
        cmd_prefix = "ssh " +host+ " \""
        cmd_postfix = "\""
        location = os.getcwd()
        if not os.path.isdir('%s/diskfill/' % location):
            os.makedirs('%s/diskfill/' % location)
        cmd_str = cmd_prefix + "dd if=/dev/zero bs=1024K count=1000 of=" +location+ "/diskfill/" + filename +cmd_postfix      
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
        if int(results['rc']) !=0:
            raise Exception('disk fill not working')

    def changetracking(self, type = 'mirror'):
        ''' Routine to inject fault that places system in change tracking'''
        tinctest.logger.info("Put system in changetracking ")
        cmd_str = 'gpfaultinjector -f filerep_consumer -m async -y fault -r %s -H ALL' %type
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
        return results['stdout']

    def is_dbdown(self):
        '''return true if system is the system is down'''
        tinctest.logger.info("checking if segments are up and in sync")
        cmd_str = "select count(*) from gp_segment_configuration where mode = 'c'"
        res = PSQL.run_sql_command(cmd_str, dbname = 'template1', flags ='-q -t')
        if int(res.strip()) > 0:
            return True
        return False

    def checkLogMessage(self):
        ''' Select from gp_toolkit log message to see if the concurrent test run resulted in No space left on device'''
        log_sql = "select substring(logmessage from 0 for 60) from gp_toolkit.__gp_log_segment_ext where logmessage like '%disabled%';"
        for i in range(1,25):
            result = PSQL.run_sql_command(log_sql, dbname = 'template1', flags ='-q -t')
            result= res.strip().split('\n')
            log_chk = False
            for logmsg in result:
                if ('write error for change tracking full log' in logmsg[0] ):
                    log_chk = True
                    break
            if log_chk == True:
                break
            sleep(2)
        if log_chk == False:
            raise Exception("Message 'changetracking disabled' not found in the logs")
    
        return True

    def remove_fillfiles(self, filename, host):
        location = os.getcwd()
        cmd_str = "ssh %s rm %s/diskfill/%s*" % (host,location, filename)

        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
        if int(results['rc']) !=0:
            raise Exception('Unable to delete the fill files')
        return

    def recover_segments(self):
        '''call gprecoverseg -a to recover down segments'''
        tinctest.logger.info("call gprecoverseg -a to recover down segments")
        cmd_str = 'gprecoverseg -a'
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
    
