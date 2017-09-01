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

class SwitchCheckpoint25To33(SwitchCheckpointTestCase):
    '''
    Testing state of transactions with faults on master after crash-recovery
    '''

    def __init__(self, methodName):
        super(SwitchCheckpoint25To33,self).__init__(methodName)
    
    def test_switch_checkpoint_25_33(self):
        '''
        @description: Tests with serial execution of pre/trigger/post sqls
        @data_provider data_types_provider
        '''
        cluster_state = self.test_data[1][0]
        fault_type = self.test_data[1][1]
        self.switch_checkpoint_serial(cluster_state, fault_type)


@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {'switch_checkpoint_25_dtm_broadcast_prepare_sync': ['sync','dtm_broadcast_prepare'],
        'switch_checkpoint_26_dtm_broadcast_commit_prepared_sync': ['sync','dtm_broadcast_commit_prepared'],
        'switch_checkpoint_27_dtm_xlog_distributed_commit_sync': ['sync','dtm_xlog_distributed_commit'],
        'switch_checkpoint_28_dtm_broadcast_prepare_change_tracking': ['change_tracking','dtm_broadcast_prepare'],
        'switch_checkpoint_29_dtm_broadcast_commit_prepared_change_tracking': ['change_tracking','dtm_broadcast_commit_prepared'],
        'switch_checkpoint_30_dtm_xlog_distributed_commit_change_tracking': ['change_tracking','dtm_xlog_distributed_commit'],
        'switch_checkpoint_31_dtm_broadcast_prepare_resync': ['resync','dtm_broadcast_prepare'],
        'switch_checkpoint_32_dtm_broadcast_commit_prepared_resync': ['resync','dtm_broadcast_commit_prepared'],
        'switch_checkpoint_33_dtm_xlog_distributed_commit_resync': ['resync','dtm_xlog_distributed_commit']
        }
    return data
