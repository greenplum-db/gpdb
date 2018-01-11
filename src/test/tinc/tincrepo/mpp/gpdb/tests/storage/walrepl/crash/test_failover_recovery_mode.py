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
import socket

import tinctest
import unittest2 as unittest
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.crash import WalReplKillProcessTestCase
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl import lib as walrepl

pgutil = GpUtility()
class WalReplKillProcessScenarioTestCase(ScenarioTestCase):

    origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY')
    def __init__(self, methodName):
        self.standby_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        self.pgdatabase = self.pgdatabase = os.environ.get('PGDATABASE')
        super(WalReplKillProcessScenarioTestCase,self).__init__(methodName)


    def setUp(self):
        pgutil.check_and_start_gpdb()
        # We should forcibly recreate standby, as it might has been promoted.
        # here we need to install locally, otherwise can not run remote sql
        pgutil.remove_standby()
        pgutil.install_standby(new_stdby_host=socket.gethostname())
        gpact_stdby = GpactivateStandby()
        gpinit_stdb = GpinitStandby()
        WalReplKillProcessTestCase.stdby_port = gpact_stdby.get_standby_port()
        WalReplKillProcessTestCase.stdby_host = gpinit_stdb.get_standbyhost()
        self.standby_dir = gpact_stdby.get_standby_dd()

    def tearDown(self):
        walrepl.cleanupFilespaces(dbname=os.environ.get('PGDATABASE')) 

    @classmethod
    def setUpClass(cls):
        pgutil.check_and_start_gpdb()
        gp_walrepl = WalReplKillProcessTestCase('initial_setup')
        gp_walrepl.initial_setup()

    @classmethod
    def tearDownClass(cls):
        pgutil.remove_standby()


    def test_failover_run__workload(self):
        ''' activate the standby, run workload, check master and standby
         integrity, currently support local standby, can not run workload
         remotely
        '''
 
        activatestdby = GpactivateStandby()
        activatestdby.activate()
      
        with walrepl.NewEnv(MASTER_DATA_DIRECTORY=self.standby_dir,
                             PGPORT=WalReplKillProcessTestCase.stdby_port,
                             PGDATABASE=self.pgdatabase) as env:
            test_case_list1 = []
            test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase")
            test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase")
            self.test_case_scenario.append(test_case_list1)

            test_case_list2 = []
            test_case_list2.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg")
            self.test_case_scenario.append(test_case_list2)

            test_case_list3 = []
            test_case_list3.append("mpp.gpdb.tests.storage.walrepl.crash.verify.verify.DataVerifyTestCase")
            self.test_case_scenario.append(test_case_list3)
        pgutil.failback_to_original_master(self.origin_mdd, WalReplKillProcessTestCase.stdby_host, self.standby_dir,WalReplKillProcessTestCase.stdby_port)        

    def test_initstandby_run_workload(self):
        #run workload while initstandby, check master mirror integrity
        pgutil.remove_standby()
         
        test_case_list0 = []
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.gpinitstandby_helper")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase")
        self.test_case_scenario.append(test_case_list0)  
    
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg") 
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.walrepl.crash.verify.verify.DataVerifyTestCase")
        self.test_case_scenario.append(test_case_list2)

    def test_initstandby_after_run_workload(self):
        #run workload before initstandby, check master mirror integrity
        pgutil.remove_standby()
    
        test_case_list0 = []
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase")
        self.test_case_scenario.append(test_case_list0)  
    
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.gpinitstandby_helper")
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg") 
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append("mpp.gpdb.tests.storage.walrepl.crash.verify.verify.DataVerifyTestCase")
        self.test_case_scenario.append(test_case_list3)
         
    def test_run_workload_with_standby(self):
        #run workload while initstandby already installed, check master mirror integrity
    
        test_case_list0 = []
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase")
        self.test_case_scenario.append(test_case_list0)  
    
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg") 
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.walrepl.crash.verify.verify.DataVerifyTestCase")
        self.test_case_scenario.append(test_case_list2)
         
    def test_run_workload_remove_standby(self):
        #run workload while removing initstandby, check master mirror integrity
    
        test_case_list0 = []
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.removestandby_helper")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase")
        self.test_case_scenario.append(test_case_list0)  
    
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg") 
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.walrepl.crash.verify.verify.DataVerifyTestCase")
        self.test_case_scenario.append(test_case_list2)
         
    def test_run_workload_before_activate_standby(self):
        #run workload while removing initstandby, check master mirror integrity
        activatestdby = GpactivateStandby()
 
        test_case_list0 = []
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase")
        test_case_list0.append("mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase")
        self.test_case_scenario.append(test_case_list0)  

        activatestdby.activate()
    
        test_case_list1 = []
        test_case_list1.append("mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg") 
        self.test_case_scenario.append(test_case_list1)
     
        test_case_list2 = []
        test_case_list2.append("mpp.gpdb.tests.storage.walrepl.crash.verify.verify.DataVerifyTestCase")
        self.test_case_scenario.append(test_case_list2)
        
        pgutil.failback_to_original_master(self.origin_mdd,WalReplKillProcessTestCase.stdby_host, self.standby_dir,WalReplKillProcessTestCase.stdby_port) 
