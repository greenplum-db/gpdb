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
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby

config = GPDBConfig()

class WalRecTestCase(MPPTestCase):
    ''' Testcases for SIGTERM on wal receiver'''

    def __init__(self, methodName):
        self.gp = GpinitStandby()
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.pgutil = GpUtility()
        super(WalRecTestCase,self).__init__(methodName)

    def setUp(self):
        #Remove standby if present
        self.gp.run(option='-r')
       
    def invoke_sigterm_and_verify(self):
        ''' Invoke sigterm on wal receiver and verify that a new process is spawned after '''
        gpact_stdby = GpactivateStandby()
        standby_host = gpact_stdby.get_current_standby()
        standby_port = gpact_stdby.get_standby_port()
        wal_rec_pid_1 = self.pgutil.get_pid_by_keyword(host=standby_host, pgport=standby_port, keyword='wal receiver process', option='')
        sig_cmd = "gpssh -h %s -e 'kill -15 %s'" % (standby_host, wal_rec_pid_1)
        cmd = Command('Issue SIGTERM to wam receiver process', cmdStr=sig_cmd)
        tinctest.logger.info ('%s' % cmd)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            return False        
        wal_rec_pid_2 = self.pgutil.get_pid_by_keyword(host=standby_host, pgport=standby_port, keyword='wal receiver process', option='')
        if wal_rec_pid_1 == wal_rec_pid_2:
            return False
        return True

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_sigterm_on_walreceiver(self):
        self.pgutil.install_standby()
        self.assertTrue(self.invoke_sigterm_and_verify())
        
