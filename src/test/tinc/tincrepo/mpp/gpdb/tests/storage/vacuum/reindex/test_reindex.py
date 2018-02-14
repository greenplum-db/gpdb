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

class ReindexConcurrencyTestCase(SQLIsolationTestCase):
    '''
    Test for REINDEX (index/table/system/database) with various concurrent transactions.
    Includes bitmap/btree/GiST tys of indexes
    For storage type AO/AOCO/Heap Relations
    '''
    sql_dir = 'concurrency/sql/'
    ans_dir = 'concurrency/expected'
    out_dir = 'concurrency/output/'
