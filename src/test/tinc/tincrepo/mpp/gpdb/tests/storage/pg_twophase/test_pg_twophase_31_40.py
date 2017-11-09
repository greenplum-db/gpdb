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
from mpp.gpdb.tests.storage.pg_twophase.pg_twophase import PgtwoPhaseTestCase

class PgtwoPhase31To40(PgtwoPhaseTestCase):
    ''' 
    Testing state of prepared transactions upon crash-recovery
    @gucs gp_create_table_random_default_distribution=off
    '''

    def __init__(self, methodName):
        super(PgtwoPhase31To40,self).__init__(methodName)
    

    def test_execute_split_sqls_31_40(self):
        ''' 
        @data_provider data_types_provider
        '''
        skip_state = self.test_data[1][0]
        cluster_state = self.test_data[1][1]
        ddl_type = self.test_data[1][2]
        fault_type = self.test_data[1][3]
        crash_type = self.test_data[1][4]
        self.execute_split_sqls(skip_state, cluster_state, ddl_type, fault_type, crash_type)
        
        
@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {'31_noskip_resync_drop_commit_gpstop_i': ['noskip','resync','drop','commit','gpstop_i'],
            '33_skip_sync_create_abort_gpstop_i': ['skip','sync','create','abort','gpstop_i'],
            '35_skip_sync_create_abort_failover_to_primary': ['skip','sync','create','abort','failover_to_primary'],
            '36_skip_sync_create_abort_failover_to_mirror': ['skip','sync','create','abort','failover_to_mirror'],
            '37_noskip_sync_create_abort_gpstop_i': ['noskip','sync','create','abort','gpstop_i'],
            '39_noskip_sync_create_abort_failover_to_primary': ['noskip','sync','create','abort','failover_to_primary'],
            '40_noskip_sync_create_abort_failover_to_mirror': ['noskip','sync','create','abort','failover_to_mirror']
            }
    return data
        

