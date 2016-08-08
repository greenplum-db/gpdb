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

from tinctest.models.scenario import ScenarioTestCase

class PGTerminateBackendTestCase(ScenarioTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    def __init__(self, methodName):
        super(PGTerminateBackendTestCase,self).__init__(methodName)

    def test_mpp21545(self):
        test_case_list0 = []
        test_case_list0.append('mpp.gpdb.tests.storage.access_methods.pg_terminate_backend.PGTerminateBackendTincTestCase.setup_table') 
        self.test_case_scenario.append(test_case_list0)          

        test_case_list1 = []
        test_case_list1.append('mpp.gpdb.tests.storage.access_methods.pg_terminate_backend.PGTerminateBackendTincTestCase.get_linenum_pglog')
        self.test_case_scenario.append(test_case_list1)

        test_case_list2 = []
        test_case_list2.append('mpp.gpdb.tests.storage.access_methods.pg_terminate_backend.PGTerminateBackendTincTestCase.backend_start') 
        test_case_list2.append('mpp.gpdb.tests.storage.access_methods.pg_terminate_backend.PGTerminateBackendTincTestCase.backend_terminate')
        self.test_case_scenario.append(test_case_list2)

        test_case_list3 = []
        test_case_list3.append('mpp.gpdb.tests.storage.access_methods.pg_terminate_backend.PGTerminateBackendTincTestCase.verify_mpp21545') 
        self.test_case_scenario.append(test_case_list3)     
