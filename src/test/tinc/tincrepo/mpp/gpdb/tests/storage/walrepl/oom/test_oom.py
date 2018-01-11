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
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.walrepl.oom import OOMClass
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

oom = OOMClass()
config = GPDBConfig()

class OOMGpstartTestCase(MPPTestCase):
    ''' Simulating oom on standby master , gpstart fails to start standby'''
    
    # Setup OOM env
    oom.setup_oom()

    def __init__(self, methodName):
        self.gpinitstandby= GpinitStandby()
        self.gputil = GpUtility()
        super(OOMGpstartTestCase,self).__init__(methodName)

    def setUp(self):
        self.gpinitstandby.run(option='-r')

    def tearDown(self):
        self.gpinitstandby.run(option='-r')

    def test_oom_with_gpstart(self):
        # Create standby
        oom.create_standby()
        # Stop cluster
        self.gputil.run('gpstop -a')
        # Run gpstart after creaitng the flag file- Presence of this flag file will return NULL for malloc simulating a no  memory scenario
        oom.touch_malloc()
        self.assertFalse(oom.startdb(), 'Standby start is expected to fail with no memory error')
        # Restart after removing the flag file
        self.assertTrue(oom.restartdb(), 'Expected a successful start of standby')

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_oom_with_psql_con(self):
        # Create standby
        oom.create_standby()
        # Stop cluster
        self.gputil.run('gpstop -a')
        # Run gpstart with wrapper
        oom.startdb()
        # OOM with psql- after adding the flag file to the standby directory to simulate OOM
        self.assertTrue(oom.psql_and_oom())
        # Start standby after free up memory
        self.assertTrue(oom.start_standby())
        
