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

import os
import tinctest
from tinctest.lib import local_path
from mpp.gpdb.tests.storage.lib import Database
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.filerep_end_to_end import FilerepTestCase
from mpp.models import MPPTestCase

class CrashRecoveryFilerepTestCase(ScenarioTestCase,MPPTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    def __init__(self, methodName):
        self.filerep = FilerepTestCase('preprocess')
        self.path = os.path.join(self.get_source_dir(), '../filerep_end_to_end/data')
        super(CrashRecoveryFilerepTestCase, self).__init__(methodName)

    def setUp(self):
        '''
        gpfdist port should be dynamically generated, this is a chance
        that it may fail to start gpfdist service as other processes are
        likely to be running on this specific port.   
        '''
        db = Database()
        db.setupDatabase('gptest')

        self.filerep.preprocess()
        self.filerep.setupGpfdist('8088', self.path)
        self.filerep.method_setup()
        super(CrashRecoveryFilerepTestCase, self).setUp()

    def tearDown(self):
        self.filerep.cleanupGpfdist('8088', self.path)
        super(CrashRecoveryFilerepTestCase, self).tearDown()

    def test_crash_recovery_filerep(self):
        # Flow of the function is:
        #  1 inject fault to skip check point
        #  2 inject fault to suspend filerep_transition_to_sync_before_checkpoint
        #  3 inject fault to suspend filerep_transition_to_sync_begin
        #  4 run the test for filerep incr mirror failover
        #  5 do stop start validation, recoverseg 
        #  6 wait for insync transition and check the integrity of the cluster
       

        tinctest.logger.info('Skip Checkpointing using fault injector.')
        
        test_list_0 = []
        test_list_0.append('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.run_pre_filerep_faults')
        self.test_case_scenario.append(test_list_0)
        
        test_list_1 = []
        test_list_1.append('mpp.gpdb.tests.storage.filerep_end_to_end.test_filerep_e2e.FilerepE2EScenarioTestCase.test_incr_mirror')
        self.test_case_scenario.append(test_list_1)

        test_list_2 = []
        test_list_2.append('mpp.gpdb.tests.storage.crashrecovery.SuspendCheckpointCrashRecovery.do_post_run_checks')
        self.test_case_scenario.append(test_list_2)
        
