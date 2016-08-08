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
from mpp.gpdb.tests.storage.persistent_tables.sqls.partition_tables.generate_partition_sqls import GeneratePartitionSqls
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from unittest import TestResult
from mpp.gpdb.tests.storage.persistent_tables import ClusterStateException
from mpp.lib.gprecoverseg import GpRecover

''' Persistent Table test :: Crash RecoveryTest '''
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
        tinctest.logger.info('Running the initial setup sql files - Done')
        tinctest.logger.info('Generating the load - sql files to be run concurrently')
        sqldatagen = GenerateSqls()
        sqldatagen.generate_sqls()
        tinctest.logger.info('Generating the load - sql files to be run concurrently - Done')
        tinctest.logger.info('Generating the sql files required for Partition table scenario')
        sql_generation = GeneratePartitionSqls()
        sql_generation.generate_sqls()
        tinctest.logger.info('Partition SQL files created.')

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


    def kill_primaries_while_in_transaction(self, skip_checkpoint = False):
        # Run skip checkpoint if needed
        if skip_checkpoint:
            test_case_list1 = []
            test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.CheckpointTest.test_skip_checkpoint')
            self.test_case_scenario.append(test_case_list1)
            
        ''' Following scenarios are run concurrently'''
        test_case_list2 = []
        # Start the Failover to mirror scenario
        test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_failover_to_mirror_during_transactions')
        # Start the load generation
        test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.sqls.run_sqls_Concurrently.SQLLoadTest')
        self.test_case_scenario.append(test_case_list2)
        
        # Recover the segments if down
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
        self.test_case_scenario.append(test_case_list3)
        
        #Check the Sate of DB and Cluster
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_CrashRecovery.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)
        
        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)

        
    def postmaster_reset_while_in_transaction(self,in_sync = True, kill_all_segment_processes = True):
        if not in_sync:
            test_case_list = []
            test_case_list.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_put_system_in_change_tracking_mode')
            self.test_case_scenario.append(test_case_list)
        
        ''' Following scenarios are run concurrently'''
        test_case_list1 = []
        if kill_all_segment_processes:
            test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_postmaster_reset_during_transaction_kill_all_segment_processes')
        else:
            test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_post_master_reset_during_transaction_kill_few_segment_processes')
        # Start the load generation
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.sqls.run_sqls_Concurrently.SQLLoadTest')
        self.test_case_scenario.append(test_case_list1)
        
        # Recover the segments if down
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
        self.test_case_scenario.append(test_case_list3)
        
        #Check the Sate of DB and Cluster
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_CrashRecovery.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)
        
        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)
    
    def test_partition_table_scenario(self):
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.sqls.partition_tables.partitionTables.PartitionTableQueries')
        self.test_case_scenario.append(test_case_list1)
            
        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.PartitionTableScenario')
        self.test_case_scenario.append(test_case_list2)
        
        # Recover the segments if down
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
        self.test_case_scenario.append(test_case_list3)
        
        #Check the Sate of DB and Cluster
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_CrashRecovery.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)
        
        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)

    def periodic_failover(self, skipckt = 'yes'):
        '''
          Test to simulate system faults induction over periodic time.
        '''
        #skip checkpoint,
        if skipckt == 'yes':
            test_case_list0 = []
            test_case_list0.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.CheckpointTest.test_skip_checkpoint')
            self.test_case_scenario.append(test_case_list0)

        #start load on database
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_kill_segments_periodically')
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.sqls.run_sqls_Concurrently.SQLLoadTest')
        self.test_case_scenario.append(test_case_list1)

        #recover segments 
        test_case_list2 = []
        if skipckt == 'yes':
            # we are using full recovery for the test "test_periodic_failover_with_skip_checkpoint"
            # since we hit "inconsistent lsn issue" (Ref. MPP-23631)
            test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery_full')
        else:
            test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
        self.test_case_scenario.append(test_case_list2)

        #Check the Sate of DB and Cluster
        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_CrashRecovery.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list5)

    def crash_system_immediately_after_checkpoint(self, mode = 'InSync'):
        '''
           Test to simulate system Crash Immediately After Checkpoint
        '''
        #skip checkpoint,
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.CheckpointTest.test_skip_checkpoint')
        self.test_case_scenario.append(test_case_list0)

        #start load on database
        test_case_list1 = []
        if mode == 'InSync':
            testMethod = 'mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_crash_after_checkpoint_insync'
        else:
            testMethod = 'mpp.gpdb.tests.storage.persistent_tables.fault.fault.FaultInjectionTests.test_crash_after_checkpoint_ct'
        test_case_list1.append(testMethod)
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.sqls.run_sqls_Concurrently.SQLLoadTest')
        self.test_case_scenario.append(test_case_list1)

        if mode == 'CT':
            #recover segments
            test_case_list2 = []
            test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
            self.test_case_scenario.append(test_case_list2)

        #Check the Sate of DB and Cluster
        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_CrashRecovery.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list5)


    def test_kill_primaries_while_in_transaction_with_checkpoint(self):
        """
        @fail_fast False
        """
        self.kill_primaries_while_in_transaction(False)

    def test_kill_primaries_while_in_transaction_with_skip_checkpoint(self):
        """
        @fail_fast False
        """
        ''' Invokes kill_primary scenario with skip checkpoint'''
        self.kill_primaries_while_in_transaction(True)

    def test_postmaster_reset_while_in_transaction_insync_kill_all(self):
        """
        @fail_fast False
        """
        ''' Sync Mode - Kill segment processes of ongoing transactions from all segments '''
        self.postmaster_reset_while_in_transaction(in_sync = True, kill_all_segment_processes = True)

    def test_postmaster_reset_while_in_transaction_insync_kill_few(self):        
        """
        @fail_fast False
        """
        ''' Sync Mode - Kill segment processes of ongoing transactions from few segments '''
        self.postmaster_reset_while_in_transaction(in_sync = True, kill_all_segment_processes = False)

    def test_postmaster_reset_while_in_transaction_out_of_sync_kill_all(self):
        """
        @fail_fast False
        """
        ''' Out of Sync Mode - Kill segment processes of ongoing transactions from all segments '''
        self.postmaster_reset_while_in_transaction(in_sync = False, kill_all_segment_processes = True)
 
    def test_postmaster_reset_while_in_transaction_out_of_sync_kill_few(self):
        """
        @fail_fast False
        """
        ''' Out of Sync Mode - Kill segment processes of ongoing transactions from few segments '''
        self.postmaster_reset_while_in_transaction(in_sync = False, kill_all_segment_processes = False)
    
    def test_periodic_failover_with_checkpoint(self):
        """
        @fail_fast False
        """
        ''' Test by skip checkpoint '''
        self.periodic_failover('no')

    def test_periodic_failover_with_skip_checkpoint(self):
        """
        @fail_fast False
        """
        ''' Test by skip checkpoint '''
        self.periodic_failover('yes')
        
    def test_crash_system_immediately_after_checkpoint_insync(self):
        """
        @fail_fast False
        """
        ''' Test to simulate system Crash Immediately After Checkpoint in Sync state '''
        self.crash_system_immediately_after_checkpoint('InSync')
    
    def test_crash_system_immediately_after_checkpoint_in_ct(self):
        """
        @fail_fast False
        """
        ''' Test to simulate system Crash Immediately After Checkpoint in CT state '''
        self.crash_system_immediately_after_checkpoint('CT')
        
