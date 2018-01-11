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
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby

class LoadClass(MPPTestCase):
    

    def __init__(self,methodName):
        self.gp =GpinitStandby()
        super(LoadClass,self).__init__(methodName)

    def run_skip(self, type):
        tinctest.logger.info("skipping checkpoint")
        cmd_str = 'gpfaultinjector -p %s -m async -H ALL -r primary -f checkpoint -y %s -o 0' % (os.getenv('PGPORT'), type)
        cmd = Command('skip_chkpoint', cmd_str)
        cmd.run(validateAfter =False)
        
    def skip_checkpoint(self):
        ''' Routine to inject fault that skips checkpointing '''
        self.run_skip('reset')
        self.run_skip('suspend')

    def init_standby(self):
        pg = GpUtility() 
        pg.install_standby()

    def cleanup(self):
        # Remove standby and reset the checkpoint fault
        self.gp.run(option='-r')
        self.run_skip('reset')
