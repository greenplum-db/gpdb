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

from mpp.models import MPPTestCase
from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib import NewEnv
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path
import tinctest

import os
import shutil
import socket
from time import sleep

pgutil = GpUtility()
pgutil.check_and_start_gpdb()
origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY')

class TakeFive(MPPTestCase):

    port = os.environ.get('PGPORT')
    mdd = os.environ.get('MASTER_DATA_DIRECTORY')

    def __init__(self, methodName):
        super(TakeFive, self).__init__(methodName)
   
    def setUp(self):
        Command('gpinitstandby -ra', 'gpinitstandby -ra').run()

    def tearDown(self):
        # failback_to_original_master responsible for removing standby, but
        # the newest activated standby has no relationship with oldest one.
        # so, need to stop the newest activated master, and fail back to oldest one
        with NewEnv(PGPORT=self.port,
                    MASTER_DATA_DIRECTORY=self.mdd):
                pgutil.failback_to_original_master(origin_mdd,socket.gethostname(),self.mdd,self.port)

    def test_run_five(self):
        for i in xrange(5):
            with NewEnv(PGPORT=self.port,
                        MASTER_DATA_DIRECTORY=self.mdd):        
                pguti = GpUtility() 
                if i == 0:
                    pguti.install_standby(socket.gethostname(), self.mdd)
                # starting from second time, init standby from new master, standby_dir will be like  master_newstandby_newstandby...
                else:
                    pguti.install_standby(socket.gethostname(), os.path.join(self.mdd,'newstandby'))
                gpact = GpactivateStandby()
                self.mdd = gpact.get_standby_dd()
                self.port = gpact.get_standby_port()
                gpact.activate()
                tinctest.logger.info("self.mdd is %s, self.port is %s"%(self.mdd, self.port))
