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
from mpp.gpdb.tests.storage.lib.uao_udf import create_uao_udf
from mpp.lib.PSQL import PSQL

class UAOHeapIsolationTestCase(SQLIsolationTestCase):
    """
    @tags ORCA
    """

    '''
    Test for heap tables with different concurrent transactions.

    This is used to compare the behavior with UAO behavior.
    '''
    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

    @classmethod
    def setUpClass(cls):
        super(UAOHeapIsolationTestCase, cls).setUpClass()
        create_uao_udf()

    def get_ans_suffix(self):
        primary_segments_count = PSQL.run_sql_command(
        "select 'primary_segment' from gp_segment_configuration  where content >= 0 and \
                role = 'p'").count('primary_segment') - 1
        if primary_segments_count > 1:
            return "%sseg" % primary_segments_count
        else:
            return None

