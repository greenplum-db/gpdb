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

import os
import tinctest

from mpp.gpdb.tests.storage.persistent_tables.sqls.InitialSetup import InitialSetup
from mpp.gpdb.tests.storage.persistent_tables.sqls.generate_sqls import GenerateSqls
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.lib.gprecoverseg import GpRecover

from mpp.gpdb.tests.storage.persistent_tables.sqls.join_query.joinQueryCreator import JoinQuery
from mpp.gpdb.tests.storage.persistent_tables import ClusterStateException

''' Persistent Table test :: Out of Memory Scenario '''
class PersistentTables(ScenarioTestCase):

    """
    
    @description Test Cases for Persistent Table testing QA-2417 - Crash RecoveryTest
    @created 2013-03-29 10:10:10
    @modified 2013-05-24 17:10:15
    @tags persistent tables schedule_persistent_tables 
    @product_version gpdb:
    """

    def __init__(self, methodName):
        super(PersistentTables, self).__init__(methodName)


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

    def set_up_join_query_env(self):
        tinctest.logger.info('Running SQL files under the setup folder')
        file1 = "sql_schema.sql"
        file2 = "sql_data_gen.sql"
        PSQL.run_sql_file(local_path('sqls/join_query/setup/' + file1))
        counter = 0
        while counter < 100:
            PSQL.run_sql_file(local_path('sqls/join_query/setup/' + file2))
            counter += 1        
        newJoinQuery = JoinQuery()
        # While running on VMs the following values of concurrency & iterations are enough to produce OOM
        # On physical m/c, these values need to be increased.
        newJoinQuery.write_query_to_file(concurrency=20,iterations=2)
        
    def out_of_memory_scenario(self, with_load = False):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.OOMScenario')
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.sqls.join_query.JoinQuery.JoinQueryTest')
        if with_load:
            test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.sqls.run_sqls_Concurrently.SQLLoadTest')
        self.test_case_scenario.append(test_case_list1)
        
        #Recover the segments if down
        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.check_db')
        test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
        self.test_case_scenario.append(test_case_list2, serial=True)
        
        #Check the Sate of DB and Cluster
        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_OOM.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list5)
     
    # Skipping the OOM Test as it causes system crash Ref. MPP-19971 and following tests from test-suite cannot complete
    # To enable the test remove "SKIP_" part from the method name
    def test_out_of_memory_scenario_wo_load(self):
        self.out_of_memory_scenario(with_load = False)
    
    def test_out_of_memory_scenario_with_load(self):
        """
        @fail_fast False
        """
        self.out_of_memory_scenario(with_load = True)
