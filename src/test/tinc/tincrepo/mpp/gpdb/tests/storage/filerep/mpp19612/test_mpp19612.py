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

from mpp.models import MPPTestCase
from tinctest.models.scenario import ScenarioTestCase

class FilerepProcArrayRemoveTestCase(ScenarioTestCase, MPPTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    def test_verify_ct_after_procarray_removal(self):
        '''
        @description : This test verifies that after the fix, the change-tracking is not getting to a deadlock.
        Refer MPP-19612 for details
        Scenario:
        1. Suspend primary at procarray_add
        2. Backend suspended 
        3. Kill corresponding mirror 
        4. Check if transition takes place
        5. Reset faults and recover
        @product_version gpdb:[4.3.4.0-], [4.2.8.3-4.2.99.99], 4.2.8.2SY5
        '''

        test_list_0 = []
        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.inject_fault',['procarray_add', 'suspend','primary']))

        sql_cmd = 'select * from pg_locks;'
        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.run_sql_in_background', [sql_cmd]))

        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.check_fault_status',['procarray_add'],{'role':'primary'}))

        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.inject_fault',['filerep_consumer', 'fault','mirror']))

        test_list_0.append('mpp.gpdb.tests.storage.lib.base.BaseClass.wait_till_change_tracking')

        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.reset_fault',['procarray_add','primary']))

        test_list_0.append('mpp.gpdb.tests.storage.lib.base.BaseClass.incremental_recoverseg')

        test_list_0.append('mpp.gpdb.tests.storage.lib.base.BaseClass.wait_till_insync')

        self.test_case_scenario.append(test_list_0, serial=True)
