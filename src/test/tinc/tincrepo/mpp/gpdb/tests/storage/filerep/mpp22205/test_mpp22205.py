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
import socket

import tinctest
import unittest2 as unittest
from tinctest.lib import local_path
from mpp.gpdb.tests.storage.lib import Database
from mpp.models import MPPTestCase
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.filerep_end_to_end import FilerepTestCase

class FilerepE2EScenarioTestCase(ScenarioTestCase, MPPTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """
    
    def __init__(self, methodName):
        self.filerep = FilerepTestCase('preprocess')
        self.path = local_path("data")
        super(FilerepE2EScenarioTestCase,self).__init__(methodName)

    def setUp(self):

        super(FilerepE2EScenarioTestCase, self).setUp()
        db = Database()
        db.setupDatabase('gptest')
        
    def tearDown(self):
        self.filerep.cleanupGpfdist('8088', self.path)
        super(FilerepE2EScenarioTestCase, self).tearDown()
         
    @classmethod
    def setUpClass(cls):
        pass

    @classmethod
    def tearDownClass(cls):
        pass

    def test_incr_primary_small_buf(self):
        '''
        @description : This test case is an addition to the 'test_incr_primary' to test resync
        phase with low shared buffer pool size (emulating a system where resync phase
        evaluates Persistent Tables larger than the total size of shared buffer pool).

        Note:- This test is useful iff the Persistent Table Page size is 32K. If the
        size changes this test needs to be changed based on the new calculations

        XXX - Find a way to assert that the page size is indeed 32K

        @product_version gpdb:(4.2.7.3-4.2], gpdb:(4.3.1.0-4.3]
        '''
        list_cl = []
        list_cl.append("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.clean_data")
        self.test_case_scenario.append(list_cl)

        list_shared_buf = []
        list_shared_buf.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.run_gpconfig", ['shared_buffers', '512kB', '512kB']))
        self.test_case_scenario.append(list_shared_buf, serial=True)

        list_gen_load = []
        list_gen_load.append("mpp.gpdb.tests.storage.filerep.mpp22205.test_misc.PrimarySyncTestCase")
        list_gen_load.append("mpp.gpdb.tests.storage.filerep_end_to_end.runcheckpoint.runCheckPointSQL.runCheckPointTestCase")
        self.test_case_scenario.append(list_gen_load, serial=True)

        list = []
        list.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.method_run_failover",['mirror']))
        list.append("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.trigger_transition")
        self.test_case_scenario.append(list,serial=True)

        list_mirror_ct = []
        list_mirror_ct.append("mpp.gpdb.tests.storage.filerep.mpp22205.test_misc.MirrorCTTestCase")
        list_mirror_ct.append("mpp.gpdb.tests.storage.filerep_end_to_end.runcheckpoint.runCheckPointSQL.runCheckPointTestCase")
        self.test_case_scenario.append(list_mirror_ct, serial=True)

        list_resync = []
        list_resync.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.run_gprecoverseg",['incr']))
        list_resync.append("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.wait_till_insync_transition")
        self.test_case_scenario.append(list_resync, serial=True)

        list_roles_swapped = []
        list_roles_swapped.append("mpp.gpdb.tests.storage.filerep.mpp22205.test_misc.RolesSwappedTestCase")
        list_roles_swapped.append("mpp.gpdb.tests.storage.filerep_end_to_end.runcheckpoint.runCheckPointSQL.runCheckPointTestCase")
        list_roles_swapped.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.run_gpconfig", ['shared_buffers', '125MB', '125MB']))
        self.test_case_scenario.append(list_roles_swapped, serial=True)

