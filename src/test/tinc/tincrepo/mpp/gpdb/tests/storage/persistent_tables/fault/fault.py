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

from time import sleep
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from mpp.gpdb.tests.storage.lib.common_utils import checkDBUp
from mpp.lib.gprecoverseg import GpRecover
from tinctest import TINCTestCase

'''
System recovery
'''
class RecoveryTest(TINCTestCase):

    def check_db(self):
        checkDBUp()

    def test_recovery(self):
        gprecover = GpRecover()
        gprecover.incremental()
        gprecover.wait_till_insync_transition()
            
    def test_recovery_full(self):
        gprecover = GpRecover()
        gprecover.full()
        gprecover.wait_till_insync_transition()
   
    def test_rebalance_segment(self):
        newfault = Fault()
        self.assertTrue(newfault.rebalance_cluster(),"Segments not rebalanced!!")
            
    def test_recovery_abort(self):
        newfault = Fault()
        sleep(100)
        newfault._run_sys_cmd('gprecoverseg -a &')
        newfault.stop_db('i')

    def test_recovery_full_abort(self):
        newfault = Fault()
        sleep(100)
        newfault._run_sys_cmd('gprecoverseg -aF &')
        newfault.stop_db('i')

'''
   DB Opration
'''
class GPDBdbOps(TINCTestCase):

    def gpstop_db(self):
        ''' Stop database with normal mode '''
        newfault = Fault()
        sleep(5)
        newfault.stop_db()
        
    def gpstop_db_immediate(self):
        ''' Stop database with immediate mode '''
        newfault = Fault()
        sleep(5)
        newfault.stop_db('i')
        
    def gpstart_db(self):
        ''' Start Database with normal mode '''
        sleep(5)
        newfault = Fault()
        newfault.start_db()
        
    def gprestart_db(self):
        ''' Restarts the Database '''
        sleep(5)
        newfault = Fault()
        newfault.restart_db()
        
    def gpstart_db_restricted(self):
        ''' Start Database with Restricted mode '''
        sleep(5)
        newfault = Fault()
        newfault.start_db('R')
