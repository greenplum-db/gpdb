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

class PgtwoPhase11To20(PgtwoPhaseTestCase):
    ''' 
    Testing state of prepared transactions upon crash-recovery
    @gucs gp_create_table_random_default_distribution=off
    '''

    def __init__(self, methodName):
        super(PgtwoPhase11To20,self).__init__(methodName)
    

    def test_execute_split_sqls_11_20(self):
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
    data = {'11_noskip_change_tracking_create_commit_gpstop_i': ['noskip','change_tracking','create','commit','gpstop_i'],
            '13_skip_resync_create_commit_gpstop_i': ['skip','resync','create','commit','gpstop_i'],
            '15_noskip_resync_create_commit_gpstop_i': ['noskip','resync','create','commit','gpstop_i'],
            '17_skip_sync_drop_commit_gpstop_i': ['skip','sync','drop','commit','gpstop_i'],
            '19_skip_sync_drop_commit_failover_to_primary': ['skip','sync','drop','commit','failover_to_primary'],
            '20_skip_sync_drop_commit_failover_to_mirror': ['skip','sync','drop','commit','failover_to_mirror']
            }
    return data
        

