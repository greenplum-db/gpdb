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
from tinctest.models.scenario import ScenarioTestCase

''' Added test for MPP:15435 '''
class SetMaxPreparedTransactionsGUC(ScenarioTestCase):
 	"""
	    
	    @description test cases for MPP-15435
	    @created 2013-05-03 10:10:10
	    @modified 2013-05-06 12:48:15
	    @tags persistent tables schedule_transactions 
	    @product_version gpdb:
	"""
	
	def test_SetMaxPreparedTransactionsGUC(self):
	    '''MPP:15435: Test boundry condition values for Max_Prepared_Transactions GUC '''
	    #1. Set max_prepared_transactions=0
 	    test_case_list1 = []
	    test_case_list1.append('mpp.gpdb.tests.storage.transaction_management.mpp15435.updateguc.UpdateGPDBGUCs.test_set_max_prepared_transactions')
	    self.test_case_scenario.append(test_case_list1)
	    
            #2. Reboot database
	    test_case_list2 = []
            test_case_list2.append('mpp.gpdb.tests.storage.transaction_management.mpp15435.updateguc.GPDBOps.test_reboot_gpdb')
	    self.test_case_scenario.append(test_case_list2)

	    #3. Test SQL Session
            test_case_list3 = []
            test_case_list3.append('mpp.gpdb.tests.storage.transaction_management.mpp15435.updateguc.GPDBOps.test_gp_segment_configuration')
            self.test_case_scenario.append(test_case_list3)

	    #4. verify GUC's Value
            test_case_list4 = []
            test_case_list4.append('mpp.gpdb.tests.storage.transaction_management.mpp15435.updateguc.UpdateGPDBGUCs.test_get_max_prepared_transactions')
            self.test_case_scenario.append(test_case_list4) 
