#!python
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
import unittest2 as unittest
from tinctest.lib import run_shell_command, local_path
from mpp.lib.PSQL import PSQL
from mpp.lib.gprecoverseg import GpRecover

class GpRecoversegRegressionTests(unittest.TestCase):

    def setUp(self):
        self.gprec = GpRecover()
        self.gphome = os.environ.get('GPHOME')

    def failover(self, type = 'mirror'):
        if type == 'mirror':
            fault_str = 'source %s/greenplum_path.sh;gpfaultinjector -f filerep_consumer  -m async -y fault -r mirror -H ALL' % self.gphome
        else:
            fault_str = 'source %s/greenplum_path.sh;gpfaultinjector -f postmaster -m async -y panic -r primary -H ALL' % self.gphome
        return run_shell_command(fault_str, cmdname = 'Run fault injector to failover')
    
    def test_incr_gprecoverseg(self):
        self.gprec.wait_till_insync_transition()
        if(self.failover()):
            self.assertTrue(self.gprec.incremental())

    def test_full_gprecoverseg(self):
        self.gprec.wait_till_insync_transition()
        if(self.failover()):
            self.assertTrue(self.gprec.full())

    def test_gprecoverseg_rebalance(self):
        self.gprec.wait_till_insync_transition()
        if(self.failover('primary')):
            PSQL.run_sql_file(local_path('mirror_failover_trigger.sql'))
            self.gprec.incremental()
            if (self.gprec.wait_till_insync_transition()):
                self.assertTrue(self.gprec.rebalance())
    
    def test_wait_till_insync(self):
        self.gprec.wait_till_insync_transition()
        if(self.failover()):
            self.gprec.incremental()
            self.assertTrue(self.gprec.wait_till_insync_transition())

