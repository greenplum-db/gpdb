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
import os

from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.genFault import Fault
from mpp.lib.config import GPDBConfig

from time import sleep
from mpp.lib.gprecoverseg import GpRecoverseg
from gppylib.commands.base import Command
from mpp.lib.gprecoverseg import GpRecover

MYD = os.path.abspath(os.path.dirname(__file__))

class GprecoversegTest(ScenarioTestCase):
    """
    
    @description This test-suite contains the automation for 'gprecoverseg' tests
    @created 2009-01-27 14:00:00
    @modified 2013-09-12 17:10:15
    @tags storage schema_topology 
    @product_version gpdb:4.2.x,gpdb:main
    """

    def __init__(self, methodName, should_fail=False):
        super(GprecoversegTest, self).__init__(methodName)

    def get_version(self):
        cmdStr = 'gpssh --version'
        cmd = Command('get product version', cmdStr=cmdStr)
        cmd.run(validateAfter=True)
        return cmd.get_results().stdout.strip().split()[2]

    def recover_segments(self,option,max_rtrycnt):
        """
        @summary: Recovers the segments and returns the status of recovery process.
        
        @param option: represents different gprecoverseg command options
        @param max_rtrycnt: the max no. of times state of cluster should be checked
        @return: Boolean value representing the status of recovery process
        """
        
        config = GPDBConfig()
        recoverseg = GpRecoverseg()
        tinctest.logger.info("Running gprecoverseg with '%s' option..."%option)
        recoverseg.run(option)
        rtrycnt = 0
        while ((config.is_not_insync_segments()) == False and rtrycnt <= max_rtrycnt):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            sleep(10)
            rtrycnt = rtrycnt + 1
        if rtrycnt > max_rtrycnt:
            return False
        else:
            return True
    def wait_till_insync_transition(self):
        self.gpr = GpRecover()
        self.gpr.wait_till_insync_transition()
            
    def check_segment_roles(self):
        """
        @summary: Checks if the segments are in preferred roles or not.
                    If not, rebalances the cluster.
        
        @return: None
        """
        
        newfault = Fault()
        # If the segments are not in preferred roles, go for rebalancing the cluster
        if newfault.check_if_not_in_preferred_role():
            tinctest.logger.warn("***** Segments not in their preferred roles : rebalancing the segments...")
            # If rebalancing downs the segments, go for incremental recovery - this is observed sometimes
            if not self.recover_segments('-r',10):
                tinctest.logger.warn("***** Segments down after rebalance : Tests cannot proceed further!!")
            # If rebalancing passes proceed for tests
            else:
                tinctest.logger.info("***** Segments successfully rebalanced : Proceeding with the tests")
        # If segments in preferred roles, proceed for the tests
        else:
            tinctest.logger.info("***** Segments in preferred roles : Proceeding with the tests")

    def check_cluster_health(self, doFullRecovery = False):
        """
        @summary: Checks for the cluster health, tries to recover and rebalance the cluster, 
                    fails the test if not able to do so 
        
        @param doFullRecovery: Boolean value which decides whether to go for full 
                                recovery or not
        @return: None
        """
        
        tinctest.logger.info("***** Checking the cluster health before starting tests")
        config = GPDBConfig()
        # If the segments are not up, go for recovery
        if not config.is_not_insync_segments():
            tinctest.logger.info("***** Starting the recovery process")
            # if incremental didn't work, go for full recovery
            if not self.recover_segments(' ',10):
                tinctest.logger.warn("***** Segments not recovered after incremental recovery")
                if doFullRecovery:                    
                    # if full also fails, the tests cannot proceed, so fail it
                    if not self.recover_segments('-F',20):
                        tinctest.logger.error("***** Segments not recovered even after full recovery - Tests cannot proceed further!!")
                        self.fail("Segments are down - Tests cannot proceed further!!")
                    # if full recovery passes, check for rebalancing the cluster
                    else:
                        tinctest.logger.info("***** Segments up after full recovery : validating their roles...")
                        self.check_segment_roles()
                else:
                    self.fail("Segments are down - Tests cannot proceed!!")
            # if incremental recovery passes, check for rebalancing the cluster
            else:
                tinctest.logger.info("***** Segments up after incremental recovery : validating their roles...")
                self.check_segment_roles()
        # If the segments are up, check for rebalancing the cluster
        else:
            tinctest.logger.info("***** Segments are up : validating their roles...")
            self.check_segment_roles()

    def setUp(self):
        super(GprecoversegTest,self).setUp()
        self.check_cluster_health()


    def test_01_run_schema_topology(self):
        """
        [feature]: Test to run the schema_topology test-suite
        
        @fail_fast False
        """
        tinctest.logger.info('***Running the schema topology test-suite before starting the tests...')
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_AllSQLsTest.AllSQLsTest')
        self.test_case_scenario.append(test_case_list1)
        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_OSSpecificSQLsTest.OSSpecificSQLsTest')
        self.test_case_scenario.append(test_case_list2)
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_GPFilespaceTablespaceTest.GPFilespaceTablespaceTest')
        self.test_case_scenario.append(test_case_list3)
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.catalog.schema_topology.test_ST_EnhancedTableFunctionTest.EnhancedTableFunctionTest')
        self.test_case_scenario.append(test_case_list4)


    def test_gprecoverseg_config_validation(self):
        """
        [feature]: Test to check if gprecoverseg sets the config to invalid state or not
        
        """
        
        # Create a fault
        test_case_list1 = []
        #fault
        test_case_list1.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_kill_primary")
        self.test_case_scenario.append(test_case_list1)
        
        # Run the invalid state recovery process
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_invalid_state_recoverseg")
        test_case_list2.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.SegmentConfigurations")
        self.test_case_scenario.append(test_case_list2)
        
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.gprestartdb')
        self.test_case_scenario.append(test_case_list3)
    
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.check_if_not_in_preferred_role')
        self.test_case_scenario.append(test_case_list4)
        
        # Check the Sate of DB and Cluster
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)
        
        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)


    def test_gprecoverseg_incr_newDir(self):
        """
        [feature]: Incremental gprecoverseg to new location: Same host, with filespaces
        
        """
        
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_kill_primary')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_recovery_with_new_loc')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.gprestartdb')
        self.test_case_scenario.append(test_case_list3)
    
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.check_if_not_in_preferred_role')
        self.test_case_scenario.append(test_case_list4)
        
        # Check the Sate of DB and Cluster
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)


    def test_failover_to_mirror(self):
        """
        [feature]:  System failover to mirror and do incremental recovery
        
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_kill_primary')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_incremental_recovery')
        self.test_case_scenario.append(test_case_list2) 

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.gprestartdb')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.check_if_not_in_preferred_role')
        self.test_case_scenario.append(test_case_list4)
        
        # Check the Sate of DB and Cluster
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)

    def test_failover_to_primary(self):
        """
        [feature]:  System failover to Primary and do incremental recovery
        
        """
        
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_kill_mirror')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_incremental_recovery')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.gprestartdb')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.check_if_not_in_preferred_role')
        self.test_case_scenario.append(test_case_list4)
        
        # Check the Sate of DB and Cluster
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)

    def test_drop_pg_dirs_on_primary(self):
        """
        [feature]:   System Failover to Mirror due to Drop pg_* dir on Primary
        
        """
        
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_drop_pg_dirs_on_primary')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_full_recovery')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.gprestartdb')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GPDBdbOps.check_if_not_in_preferred_role')
        self.test_case_scenario.append(test_case_list4)
        
        # Check the Sate of DB and Cluster
        test_case_list5 = []
        test_case_list5.append("mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list6)

    def test_incremental_recovery_with_symlinks(self):
        """
        [feature]: Incremental recovery when the data directory is a symlink inctead of a normal directory
        
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_create_symlink_for_seg')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_incremental_recovery')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_remove_symlink_for_seg')
        self.test_case_scenario.append(test_case_list5)

    def test_incremental_recovery_without_symlinks(self):
        """
        [feature]: Incremental recovery when the data directory is a symlink instead of a regular directory
        
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_incremental_recovery')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list3)

    def test_full_recovery_with_symlinks(self):
        """
        [feature]: Full recovery when data directory is a symlink instead of a regular directory
        
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_create_symlink_for_seg')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_full_recovery')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_remove_symlink_for_seg')
        self.test_case_scenario.append(test_case_list5)
        
    def test_full_recovery_without_symlinks(self):
        """
        [feature]: Full recovery when data directory is a symlink instead of a regular directory
        
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_full_recovery')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list3)

    def test_skip_persistent_check_with_incremental_recovery(self):
        """
        [feature]: skip the persistent table check when the user does not supply the --persistent-check option 
        
        @product_version gpdb: [4.3.5.0 -]
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_incremental_recovery_skip_persistent_tables_check')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list3)

    def test_skip_persistent_check_with_full_recovery(self):
        """
        [feature]: skip the persistent table check when the user does not supply the --persistent-check option 
        
        @product_version gpdb: [4.3.5.0 -]
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_full_recovery_skip_persistent_tables_check')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list3)

    def test_persistent_check_with_incremental_recovery(self):
        """
        [feature]: Perform the persistent table check when the persistent tables have been corrupted 
        
        @product_version gpdb: [4.3.5.0 -]
        """
        
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_corrupt_persistent_tables')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_incremental_recovery_with_persistent_tables_corruption')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_rebuild_persistent_tables')
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_incremental_recovery_with_persistent_tables_corruption')
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list6)

    def test_persistent_check_with_full_recovery(self):
        """
        [feature]: Perform the persistent table check when the persistent tables have been corrupted 
        
        @product_version gpdb: [4.3.5.0 -]
        """
        
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_corrupt_persistent_tables')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_use_gpfaultinjector_to_mark_segment_down')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_full_recovery_with_persistent_tables_corruption')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_rebuild_persistent_tables')
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_full_recovery_with_persistent_tables_corruption')
        self.test_case_scenario.append(test_case_list5)

        test_case_list6 = []
        test_case_list6.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list6)

    def test_clean_shared_mem(self):
        """
        [feature]: Check the shared memory cleanup for gprecoverseg 
        
        @product_version gpdb: [4.3.5.0 -]
        """

        version = self.get_version()
        if version.startswith('4.2'):
            self.skipTest('Skipping test for 4.2')

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_kill_primary')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_wait_till_segments_in_change_tracking')

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_incremental_recovery')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list4)

        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_shared_mem_is_cleaned')
        self.test_case_scenario.append(test_case_list5)

    def test_start_transaction(self):
        """
        [feature]: Check if we can start a transaction successfully before establishing a connection to database
        
        @product_version gpdb: [4.3.4.1 -]
        """

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_kill_primary_group')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.FaultInjectorTestCase.test_wait_till_segments_in_change_tracking')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.fault.GprecoversegClass.test_do_incremental_recovery')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.test_gprecoverseg.GprecoversegTest.wait_till_insync_transition')
        self.test_case_scenario.append(test_case_list4)
