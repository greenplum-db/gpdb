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
import shutil
import tinctest
import tinctest
from gppylib.commands.base import Command
        
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby

class SwitchClass(MPPTestCase):
    
    DESTDIR = 'base'

    def __init__(self,methodName):
        self.standby = Standby(self.DESTDIR, 5433)
        super(SwitchClass,self).__init__(methodName)

    def run_pg_basebackup(self):
        shutil.rmtree(self.DESTDIR, True)
        self.standby.create()
        
    def start_standby(self):
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())
        self.assertTrue(self.check_walreceiver())

    def check_walreceiver(self):
        grep_cmd = 'ps -ef|grep %s|grep -v grep' % ('wal receiver')
        cmd = Command(name='Check if no wal receiver process', cmdStr=grep_cmd)
        tinctest.logger.info ('%s' % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        if result.rc !=0:
            return True
        return False
