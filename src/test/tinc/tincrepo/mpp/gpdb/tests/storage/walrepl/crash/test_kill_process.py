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

from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.crash import WalReplKillProcessTestCase
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl import lib as walrepl

import os

pgutil = GpUtility()

class WalReplKillProcessScenarioTestCase(ScenarioTestCase):

    """
    Please export PGDATABASE=walrepl
    """
     
    def setUp(self):
        super(WalReplKillProcessScenarioTestCase, self).setUp()
        pgutil.check_and_start_gpdb()
        # We should forcibly recreate standby, as it might has been promoted.
        pgutil.remove_standby()
        pgutil.install_standby()
        gpact_stdby = GpactivateStandby()
        gpinit_stdb = GpinitStandby()
        WalReplKillProcessTestCase.stdby_port = gpact_stdby.get_standby_port()
        WalReplKillProcessTestCase.stdby_host = gpinit_stdb.get_standbyhost()

    def tearDown(self):
        walrepl.cleanupFilespaces(dbname=os.environ.get('PGDATABASE'))
        super(WalReplKillProcessScenarioTestCase, self).tearDown()

    @classmethod
    def setUpClass(cls):
        super(WalReplKillProcessScenarioTestCase,cls).setUpClass()
        gp_walrepl = WalReplKillProcessTestCase('initial_setup')
        pgutil.check_and_start_gpdb()
        gp_walrepl.initial_setup()


    @classmethod
    def tearDownClass(cls):
        pgutil.remove_standby()
        super(WalReplKillProcessScenarioTestCase, cls).tearDownClass()


    def test_kill_walsender(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.kill_walsender_check_postmaster_reset') 
        self.test_case_scenario.append(test_case_list0)     

    def test_kill_walreceiver(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase') 
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase')
        self.test_case_scenario.append(test_case_list0)
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.kill_walreceiver')
        self.test_case_scenario.append(test_case_list0)
        
        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_stdby_stop')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.start_stdby')
        self.test_case_scenario.append(test_case_list2)
     
        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.verify_standby_sync')
        self.test_case_scenario.append(test_case_list3) 

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg')
        self.test_case_scenario.append(test_case_list4)

    def test_kill_transc_backend(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.run_transaction_backend')
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.kill_transc_backend_check_reset')
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg')
        self.test_case_scenario.append(test_case_list1) 
       

    def test_kill_walstartup(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.dml.test_dml.DMLTestCase')
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.ddl.test_ddl.DDLTestCase')
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.kill_walstartup')
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_stdby_stop')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.start_stdby')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.verify_standby_sync')
        self.test_case_scenario.append(test_case_list3) 

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_mirror_seg')
        self.test_case_scenario.append(test_case_list4)


    def test_run_gpstart_kill_stdby_postmaster(self):
        ''' kill the standby postmaster process while running gpstart, should not affect other segments'''
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.gpstop_helper')
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.gpstart_helper')
        test_case_list1.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.kill_standby_postmaster')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.walrepl.crash.WalReplKillProcessTestCase.check_gpdb_status')
        self.test_case_scenario.append(test_case_list2)
