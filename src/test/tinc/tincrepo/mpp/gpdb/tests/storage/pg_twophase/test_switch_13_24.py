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
from mpp.gpdb.tests.storage.pg_twophase.switch_checkpoint import SwitchCheckpointTestCase

class SwitchChekpoint13To24(SwitchCheckpointTestCase):
    '''
    Testing state of transactions with faults on master after crash-recovery
    @gucs gp_create_table_random_default_distribution=off
    '''

    def __init__(self, methodName):
        super(SwitchChekpoint13To24,self).__init__(methodName)
    
    def test_switch_checkpoint_13_24(self):
        ''' 
        @data_provider data_types_provider
        '''
        cluster_state = self.test_data[1][0]
        fault_type = self.test_data[1][1]
        crash_type = self.test_data[1][2]
        self.switch_checkpoint(cluster_state, fault_type, crash_type)
                
@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {'switch_checkpoint_13_dtm_broadcast_commit_prepared_gpstop_i_change_tracking': ['change_tracking','dtm_broadcast_commit_prepared','gpstop_i'],
        'switch_checkpoint_14_dtm_broadcast_commit_prepared_gpstop_a_change_tracking': ['change_tracking','dtm_broadcast_commit_prepared','gpstop_a'],

        'switch_checkpoint_15_dtm_broadcast_commit_prepared_gpstop_i_resync': ['resync','dtm_broadcast_commit_prepared','gpstop_i'],
        'switch_checkpoint_16_dtm_broadcast_commit_prepared_gpstop_a_resync': ['resync','dtm_broadcast_commit_prepared','gpstop_a'],

        'switch_checkpoint_17_dtm_xlog_distributed_commit_gpstop_i_sync': ['sync','dtm_xlog_distributed_commit','gpstop_i'],
        'switch_checkpoint_18_dtm_xlog_distributed_commit_gpstop_a_sync': ['sync','dtm_xlog_distributed_commit','gpstop_a'],
        'switch_checkpoint_19_dtm_xlog_distributed_commit_failover_to_primary_sync': ['sync','dtm_xlog_distributed_commit','failover_to_primary'],
        'switch_checkpoint_20_dtm_xlog_distributed_commit_failover_to_mirror_sync': ['sync','dtm_xlog_distributed_commit','failover_to_mirror'],

        'switch_checkpoint_21_dtm_xlog_distributed_commit_gpstop_i_change_tracking': ['change_tracking','dtm_xlog_distributed_commit','gpstop_i'],
        'switch_checkpoint_22_dtm_xlog_distributed_commit_gpstop_a_change_tracking': ['change_tracking','dtm_xlog_distributed_commit','gpstop_a'],

        'switch_checkpoint_23_dtm_xlog_distributed_commit_gpstop_i_resync': ['resync','dtm_xlog_distributed_commit','gpstop_i'],
        'switch_checkpoint_24_dtm_xlog_distributed_commit_gpstop_a_resync': ['resync','dtm_xlog_distributed_commit','gpstop_a']
        }
    return data
