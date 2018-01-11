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

from time import sleep
from datetime import datetime

from mpp.gpdb.tests.storage.filerep.Filerep_Resync.fault.genFault import Fault
from tinctest import TINCTestCase

'''
Test to invoke the faults
'''
class ProcessKill(TINCTestCase):
    @classmethod
    def setUpClass(cls):
        super(ProcessKill,cls).setUpClass()
        tinctest.logger.info('Test to kill all the mirror processes')
    
    def test_kill_mirror_processes(self):
        newfault = Fault()
        # Sleep introduced in order for the Transactions to start first
        sleep(15)
        newfault.kill_processes_with_role('m')
        
    
class Recovery(TINCTestCase):
    @classmethod
    def setUpClass(cls):
        super(Recovery,cls).setUpClass()
        tinctest.logger.info('Test to run the recovery process')
        
    def test_run_recovery(self):
        newfault = Fault()
        newfault.run_recovery()

class Health(TINCTestCase):
    @classmethod
    def setUpClass(cls):
        super(Health,cls).setUpClass()
        tinctest.logger.info('Test to check if the mirror segments are up or not')

    def test_check_mirror_segments(self):
        wait_counter = 1
        max_count = 5
        newfault = Fault()
        
        while(wait_counter <= max_count):
            check_mirrors = newfault.are_mirrors_up()
            if(check_mirrors):
                tinctest.logger.info('Mirrors are up..')
                return
            tinctest.logger.info('Waiting [' + str(wait_counter) + '] for the mirrors segments to come up')
            sleep(10)
            wait_counter += 1
        
        tinctest.logger.info('\nReached the wait counter : '+str(max_count))
        self.fail('Mirrors are down even after recovery')
        
        
        
    

