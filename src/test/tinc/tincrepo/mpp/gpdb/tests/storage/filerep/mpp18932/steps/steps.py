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

import tinctest

from mpp.gpdb.tests.storage.filerep.mpp18932.steps import Steps
from tinctest.lib import local_path
from tinctest import TINCTestCase

from mpp.lib.PSQL import PSQL

class TestSteps(TINCTestCase):

    @classmethod
    def setUpClass(cls):
        tinctest.logger.info('Running Verification...')

    def test_change_tracking(self):
        newfault = Steps()
        newfault.changetracking( type = 'mirror')
        while (newfault.is_dbdown() == False):
            tinctest.logger.info("waiting for DB to go into change tracking")
            sleep(10)
        return

    def test_fillDisk(self):
        df = Steps()
        hosts = df.get_segments()
        disk = os.getcwd().split('/')[1]
        for host in hosts:
            disk_usage = df.checkDiskUsage(host[0], disk)
            i = 0
            while(int(disk_usage.strip()) >1000000):
                filename = 'new_space_%s' % i
                df.fillDisk(filename, host[0])
                i +=1
                disk_usage = df.checkDiskUsage(host[0], disk)

    def test_checkLog_recover(self):
        '''Checks if change_tracking disabled message is printed to the log, 
           if returns true, remove the fill files , run recoverseg '''
        tinctest.logger.info('coming here in chacklog')
        cl = Steps()
        if(cl.checkLogMessage()):
            tinctest.logger.info('coming in if')
            hosts = cl.get_segments()
            for host in hosts:
                cl.remove_fillfiles('new_space', host[0])
            cl.recover_segments()
            while(cl.is_dbdown() == True):
                tinctest.logger.info('Waiting for DB to be in sync')        
            
