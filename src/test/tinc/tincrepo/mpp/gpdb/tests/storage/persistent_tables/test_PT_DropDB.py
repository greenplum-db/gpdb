"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

from mpp.gpdb.tests.storage.persistent_tables.sqls.InitialSetup import InitialSetup
from mpp.gpdb.tests.storage.persistent_tables.sqls.generate_sqls import GenerateSqls
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from mpp.gpdb.tests.storage.persistent_tables import ClusterStateException
from mpp.lib.gprecoverseg import GpRecover


''' Persistent Tables test :: Drop DB '''
class PersistentTables(ScenarioTestCase):

    """
    
    @description Test Cases for Persistent Table testing QA-2417 - Crash RecoveryTest
    @created 2013-03-29 10:10:10
    @modified 2013-05-24 17:10:15
    @tags persistent tables schedule_persistent_tables 
    @product_version gpdb:
    """

    @classmethod
    def setUpClass(cls):
        super(PersistentTables,cls).setUpClass()
        tinctest.logger.info('Setup Database ...')
        setdb = Fault()
        setdb.create_db()

        tinctest.logger.info('Running the initial setup sql files')
        setup = InitialSetup()
        setup.createSQLFiles()
        setup.runSQLFiles()
        tinctest.logger.info('Generating the load - sql files to be run concurrently')
        sqldatagen = GenerateSqls()
        sqldatagen.generate_sqls()

    # Replacing the setUp method with the following one, as setUp method is called twice redundantly
    def setUp(self):
        ''' Need to rebalance cluster as primary segments are killed during test'''
        super(PersistentTables,self).setUp()
        tinctest.logger.info('***Rebalancing cluster state***')
        fault_recovery = Fault()
        if not fault_recovery.rebalance_cluster():
            raise ClusterStateException("**FATAL!! Cluster rebalancing failed - segments went down after \
                                       gprecoverseg -ar, even incremental recovery couldn't bring the segments up. \
                                       Cannot proceed with the tests!! ")
    def wait_till_insync_transition(self):
        self.gpr = GpRecover()
        self.gpr.wait_till_insync_transition()


    def test_drop_db_when_rel_file_moved_out(self):
        ''' Test drop_db by moving relfile to other location '''
        #1. Create objects and load data
        #Done in setUpClass

        #2. Do fault test
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.DropDBTest.test_drop_db')
        self.test_case_scenario.append(test_case_list1)

        #Check the Sate of DB and Cluster
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list2)
        
        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_DropDB.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list4)
