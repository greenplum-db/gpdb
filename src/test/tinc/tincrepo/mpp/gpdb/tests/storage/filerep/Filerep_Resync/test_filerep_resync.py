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

from tinctest.lib import local_path
from tinctest.models.scenario import ScenarioTestCase
from tinctest.main import TINCException
from mpp.lib.PSQL import PSQL
from mpp.lib.gprecoverseg import GpRecover


class FilerepResyncException(TINCException): pass

'''
Filerep Resync scenario
'''
class FilerepResync(ScenarioTestCase):
    """
    
    @description test cases for MPP-11167
    @created 2013-03-15 10:10:10
    @modified 2013-05-07 17:10:15
    @tags persistent tables schedule_filerep
    @product_version gpdb:
    """

    @classmethod
    def setUpClass(cls):
        super(FilerepResync,cls).setUpClass()
        tinctest.logger.info('Setting up the filerep resync test.')

    def wait_till_insync_transition(self):
        self.gpr = GpRecover()
        self.gpr.wait_till_insync_transition()
        
    def test_filerep_resysnc(self):
        
        #Step 1: Create an append-only table
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.schema.SchemaTest.AOTable")
        self.test_case_scenario.append(test_case_list1)
        
        #Step 2:1 Begin a transaction & insert values into created table
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.runsql.TransactionTest.Transaction")
        #Step 2:2 Start a concurrent process to kill all the mirror processes.
        #            It should start only after the begin & insert are performed    
        test_case_list2.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.fault.FaultTest.ProcessKill")
        self.test_case_scenario.append(test_case_list2)
        
        #Step 3: Check the persistent table for duplicate entries
        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.schema.SchemaTest.DuplicateEntries.test_duplicate_entries_after_hitting_fault")
        self.test_case_scenario.append(test_case_list3)
        
        #Step 4: Perform incremental recovery 
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.fault.FaultTest.Recovery")
        self.test_case_scenario.append(test_case_list4)
        
        #Step 5: Check if the mirror segments are up or not
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.fault.FaultTest.Health")
        self.test_case_scenario.append(test_case_list5)
        
        #Step 6: Re-check the persistent table for duplicate entries
        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.schema.SchemaTest.DuplicateEntries.test_duplicate_entries_after_recovery")
        self.test_case_scenario.append(test_case_list6)
        
        #Step 7: Check the Sate of DB and Cluster
        test_case_list7 = []
        test_case_list7.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list7)
        
        test_case_list8 = []
        test_case_list8.append("mpp.gpdb.tests.storage.filerep.Filerep_Resync.test_filerep_resync.FilerepResync.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list8)
        
        test_case_list9 = []
        test_case_list9.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list9)

