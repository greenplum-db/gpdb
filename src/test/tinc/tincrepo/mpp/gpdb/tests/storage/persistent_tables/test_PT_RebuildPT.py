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

from mpp.gpdb.tests.storage.persistent_tables.sqls.InitialSetup import InitialSetup
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from mpp.gpdb.tests.storage.persistent_tables import ClusterStateException

from mpp.lib.gprecoverseg import GpRecover

''' Persistent Tables test :: Rebuild persistent tables '''
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

    ''' Global Persistent table Rebuild -Test Enchantment ParisTX-PT '''
    def rebuild_persistent_table_objects(self, type = 'master'):
        ''' Rebuild Persistent Object '''
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.PTRebuild.persistent_rebuild_scenario.RebuildPersistentObjectsTest.test_rebuild_persistent_objects_%s' %type)
        self.test_case_scenario.append(test_case_list1)
        
        #Check the Sate of DB and Cluster
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list2)
        
        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_RebuildPT.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list3)
        
        test_case_list4 = []
        test_case_list4.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list4)

    def wait_till_insync_transition(self):
        self.gpr = GpRecover()
        self.gpr.wait_till_insync_transition()

    def test_rebuild_persistent_objects_master(self):
        ''' Rebuild Persistent Object on Master '''
        self.rebuild_persistent_table_objects('master')
        
    def test_rebuild_persistent_objects_segment(self):
        ''' Rebuild Persistent Object on Segment '''
        self.rebuild_persistent_table_objects('segment')

    # regarding  "test_abort_pt_rebuild": this sets up a race between gpstop and doing up a `select * from
    # gp_persistent_reset_all(); select * from gp_persistent_build_all(true);` the Storage team
    # indicated that this scenario seems unnecessary (users do not generally use pt_rebuild,
    # In other words, the test may create a repeatable red, but it isn't something currently judged worthy of inclusion.
    def _unused_test_abort_pt_rebuild(self):
        ''' Abort Persistent Object Rebuild '''
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.PTRebuild.persistent_rebuild_scenario.AbortRebuildPersistentObjectsTest.test_stop_db')
        test_case_list1.append('mpp.gpdb.tests.storage.persistent_tables.PTRebuild.persistent_rebuild_scenario.AbortRebuildPersistentObjectsTest.test_rebuild_persistent_objects')
        self.test_case_scenario.append(test_case_list1)

        #Start Database
        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.GPDBdbOps.gpstart_db')
        self.test_case_scenario.append(test_case_list2)

        #Issue gpcheckcat
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog')
        self.test_case_scenario.append(test_case_list3)

        #Do recovery 
        #gprecoverseg Incr
        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_recovery')
        self.test_case_scenario.append(test_case_list4)

        #Re-balance segments to rebuild PT
        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.storage.persistent_tables.fault.fault.RecoveryTest.test_rebalance_segment')
        self.test_case_scenario.append(test_case_list5)

        #Do PT rebuild 
        test_case_list6 = []
        test_case_list6.append('mpp.gpdb.tests.storage.persistent_tables.PTRebuild.persistent_rebuild_scenario.RebuildPersistentObjectsTest.test_rebuild_persistent_objects_segment')
        self.test_case_scenario.append(test_case_list6)

        #Check the Sate of DB and Cluster
        test_case_list7 = []
        test_case_list7.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_catalog")
        self.test_case_scenario.append(test_case_list7)
        
        test_case_list8 = []
        test_case_list8.append("mpp.gpdb.tests.storage.persistent_tables.test_PT_RebuildPT.PersistentTables.wait_till_insync_transition")
        self.test_case_scenario.append(test_case_list8)
        
        test_case_list9 = []
        test_case_list9.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.check_mirrorintegrity")
        self.test_case_scenario.append(test_case_list9)
