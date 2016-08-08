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

from mpp.gpdb.tests.storage.lib.sql_isolation_testcase import SQLIsolationTestCase
from mpp.models import SQLTestCase

class QueryFinishTestCase(SQLTestCase):
    """
    
    @db_name queryfinish
    @description Query-finish tests
    @created 2014-04-17 00:00:00
    @modified 2014-04-17 00:00:00
    @tags dispatch limit
    @product_version gpdb: [4.3.3.0-]
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

class QuerywithLockfromMaster(SQLIsolationTestCase):
    """
    @description: A limit query when the table has a lock on master
    @product_version gpdb: [4.3.3.0-]
    """
    sql_dir = 'sql_lk/'
    ans_dir = 'expected_lk/'
    out_dir = 'output/'
