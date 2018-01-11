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
import socket

import tinctest
import unittest2 as unittest
from tinctest.lib import local_path
from mpp.gpdb.tests.storage.lib import Database
from mpp.models import MPPTestCase
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.filerep_end_to_end import FilerepTestCase

class FilerepMiscTestCase(ScenarioTestCase, MPPTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    def test_verify_ctlog_xlog_consistency(self):
        '''
        @description : This test verifies consistency between xlog and changetracking logs.
        The consistency can be lost due to non synchronous write patterns of these logs.
        This can cause resync phase to fail due to inconsistency seen between actual data and changetracking
        logs. Refer to MPP-23631 for more explanation.

        The test increases the size of wal buffer to reduce frequency of writes to disk. This helps to push
        changetracking log more frequently and thus tries to create inconsistency between them.
        If the system works fine, it shouldn't get affected due to this behaviour. That's when the test passes.
        @product_version gpdb:(4.3.1.0-4.3], gpdb:(4.2.8.0-4.2]
        '''
        list_wal_buf = []
        list_wal_buf.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.run_gpconfig", ['wal_buffers', '512', '512']))
        self.test_case_scenario.append(list_wal_buf, serial=True)

        list_gen_load = []
        list_gen_load.append("mpp.gpdb.tests.storage.filerep.mpp23631.test_misc.ctlog_xlog_cons_setup")
        list_gen_load.append("mpp.gpdb.tests.storage.filerep_end_to_end.runcheckpoint.runCheckPointSQL.runCheckPointTestCase")
        self.test_case_scenario.append(list_gen_load, serial=True)

        list = []
        list.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.method_run_failover",['primary']))
        list.append("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.trigger_transition")
        self.test_case_scenario.append(list,serial=True)

        list_ct = []
        list_ct.append("mpp.gpdb.tests.storage.filerep_end_to_end.runcheckpoint.runCheckPointSQL.runCheckPointTestCase")
        list_ct.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.inject_fault", ['checkpoint', 'async', 'skip', 'primary']))
        self.test_case_scenario.append(list_ct,serial=True)

        list_ct = []
        list_ct.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.inject_fault", ['fault_in_background_writer_main', 'async', 'suspend', 'primary']))
        self.test_case_scenario.append(list_ct,serial=True)

        list_ct = []
        list_ct.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.inject_fault", ['appendonly_insert', 'async', 'panic', 'primary', 'all', 'ao']))
        list_ct.append("mpp.gpdb.tests.storage.filerep.mpp23631.test_misc.ctlog_xlog_cons_ct")
        self.test_case_scenario.append(list_ct,serial=True)

        list_ct_post_reset = []
        list_ct_post_reset.append("mpp.gpdb.tests.storage.filerep.mpp23631.test_misc.ctlog_xlog_cons_post_reset_ct")
        list_ct_post_reset.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.run_gprecoverseg",['incr']))
        list_ct_post_reset.append("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.wait_till_insync_transition")
#       list_ct_post_reset.append("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.check_mirror_seg")
        list_ct_post_reset.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.do_gpcheckcat",{'outputFile':'test_verify_ctlog_xlog_consistency.out'}))
        self.test_case_scenario.append(list_ct_post_reset,serial=True)

        list_cleanup = []
        list_cleanup.append("mpp.gpdb.tests.storage.filerep.mpp23631.test_misc.ctlog_xlog_cons_cleanup")
        list_cleanup.append(("mpp.gpdb.tests.storage.filerep_end_to_end.FilerepTestCase.run_gpconfig", ['wal_buffers', '8', '8']))
        self.test_case_scenario.append(list_cleanup, serial=True)

