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
from mpp.gpdb.tests.storage.lib.sql_isolation_testcase import SQLIsolationTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.vacuum.reindex import Reindex

class ReindexTestCase(ScenarioTestCase, MPPTestCase):

    def test_reindex_scenarios(self):
        '''
        This test currently verifies following situations --
        1. Table dropped during reindex database should not fail. Drop table transaction will either succeed or
        block. But in any case reindex database will proceed without failure.
        2. Reindex table should be able to retain the system in consistent mode even if a new index is added
        concurrently.
        '''

        list_setup = []
        list_setup.append("mpp.gpdb.tests.storage.vacuum.reindex.scenario.test_scenario.reindex_stp")
        self.test_case_scenario.append(list_setup, serial=True)

        list_drop_table_reindex_db = []
        list_drop_table_reindex_db.append("mpp.gpdb.tests.storage.vacuum.reindex.scenario.test_scenario.reindex_db")
        list_drop_table_reindex_db.append("mpp.gpdb.tests.storage.vacuum.reindex.scenario.test_scenario.drop_obj")
        self.test_case_scenario.append(list_drop_table_reindex_db, serial=False)

        list_add_index_reindex_table = []
        list_add_index_reindex_table.append("mpp.gpdb.tests.storage.vacuum.reindex.scenario.test_scenario.reindex_rel")
        list_add_index_reindex_table.append("mpp.gpdb.tests.storage.vacuum.reindex.scenario.test_scenario.add_index")
        self.test_case_scenario.append(list_add_index_reindex_table, serial=False)

        list_verify = []
        list_verify.append("mpp.gpdb.tests.storage.vacuum.reindex.scenario.test_scenario.reindex_verify")
        self.test_case_scenario.append(list_verify, serial=True)

class ReindexConcurrencyTestCase(SQLIsolationTestCase):
    '''
    Test for REINDEX (index/table/system/database) with various concurrent transactions.
    Includes bitmap/btree/GiST tys of indexes
    For storage type AO/AOCO/Heap Relations
    '''
    sql_dir = 'concurrency/sql/'
    ans_dir = 'concurrency/expected'
    out_dir = 'concurrency/output/'
