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
from time import sleep
import socket
import unittest2 as unittest
import tinctest
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby

config = GPDBConfig()

class CancelBkndTestCase(MPPTestCase):
    ''' Testcases for pg_cancel_backed and pg_terminate_backed on wal sender'''

    def __init__(self, methodName):
        self.gp = GpinitStandby()
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.standby = self.gp.get_standbyhostnode()
        super(CancelBkndTestCase,self).__init__(methodName)

    def setUp(self):
        #Remove standby if present
        self.gp.run(option='-r')
       
        
    def get_walsender_pid(self):
        ''' get wal sender pid '''
        pid_cmd = "ps -ef|grep \'%s\' |grep -v grep"  %  ('wal sender')
        cmd = Command('Get the pid of the wal sender process', cmdStr = pid_cmd)
        tinctest.logger.info ('%s' % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        while(result.rc !=0):
            cmd.run(validateAfter=False)
            result = cmd.get_results()
        tinctest.logger.info(result)
        pid = result.stdout.splitlines()[0].split()[1].strip()
        return pid


    def cancel_query(self,query):
        res = 't' 
        count = 0 
        while(res.strip() != 'f'):
            res = PSQL.run_sql_command(query, dbname='postgres', flags = '-q -t')
            if res.strip() == 'f':
                return True
            else:
                sleep(2)
                count+=1
                if count > 120 :
                    return False

    def terminate_backend(self):
        wal_pid_1 = self.get_walsender_pid()
        psql_command = "select pg_terminate_backend('%s')" % wal_pid_1
        return(self.cancel_query(psql_command))
        
    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_terminate_bknd_wal_sender(self):
        self.gp.create_dir_on_standby(self.standby, os.path.split(self.mdd)[0])
        self.assertTrue(self.gp.run(option = '-s %s' % self.standby))
        self.assertTrue(self.terminate_backend())
        
