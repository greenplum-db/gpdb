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

import tinctest
import unittest2 as unittest
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.gpcrondump import BkupRestore
from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby

config = GPDBConfig()

class BackupRestoreTestCase(BkupRestore):
    ''' gpcrondump from the primary master, gpdbrestore from the standby master'''

    def __init__(self, methodName):
        super(BackupRestoreTestCase,self).__init__(methodName)

    def setUp(self):
        self.gp = GpinitStandby()
        self.gp.run(option='-r')

    def tearDown(self):
        self.failback()

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_backup_restore(self):
        # Create standby if not present
        Command('createdb bkdb','dropdb bkdb; createdb bkdb').run()
        self.create_standby()
        # Run workload
        self.run_workload('sql')
        # Create backup
        self.assertTrue(self.run_backup())
        # Activate standby
        gpac = GpactivateStandby()
        gpac.activate()        
        # Restore from new master
        self.assertTrue(self.run_restore())
