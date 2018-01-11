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
import tinctest
import unittest2 as unittest
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby


class OODTestCase(ScenarioTestCase):
    ''' Standby OOD when transactions in progress, standby need to be restaerted'''

    def __init__(self, methodName):
        self.gp = GpinitStandby()
        super(OODTestCase,self).__init__(methodName)

    def setUp(self):
        #Remove standby if present
        self.gp.run(option='-r')
        
       
    def tearDown(self):
        #Remove standby 
        self.gp.run(option='-r')
        
    
    @unittest.skipIf(os.uname()[0] == 'Darwin', "Skipping this test on OSX")   
    def Dtest_ood_on_standby(self):
        
        
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.walrepl.ood.OODClass.initiate_standby')
        self.test_case_scenario.append(test_case_list0)

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.walrepl.ood.OODClass.filldisk')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.walrepl.ood.sql.runsql.RunWorkload')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.walrepl.ood.OODClass.check_standby')
        self.test_case_scenario.append(test_case_list3)

        test_case_list4 = []
        test_case_list4.append('mpp.gpdb.tests.storage.walrepl.ood.OODClass.restart_standby')
        self.test_case_scenario.append(test_case_list4)
        
        test_case_list5 = []
        test_case_list5.append('mpp.gpdb.tests.storage.walrepl.ood.OODClass.cleanup')
        self.test_case_scenario.append(test_case_list5)

