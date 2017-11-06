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
from mpp.models import MPPTestCase
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.PSQL import PSQL

class test_mpp15834(MPPTestCase):

    '''
    
    @tags mpp-15834
    @product_version gpdb: [4.2-4.2.99.99], (4.3.1.0-main]
    '''

    def setUp(self):
        self.filereputil = Filerepe2e_Util()

    def tearDown(self):
        self.filereputil.inject_fault(f='cdb_copy_start_after_dispatch', y='reset', seg_id=1)

    def test_mpp15834(self):
        # setup
        PSQL.run_sql_command("""
          DROP TABLE IF EXISTS mpp15834;
          CREATE TABLE mpp15834(a int, b int);
          INSERT INTO mpp15834 SELECT i, i * 10 FROM generate_series(1, 100)i;
        """)

        self.filereputil.inject_fault(f='cdb_copy_start_after_dispatch', y='reset', seg_id=1)
        self.filereputil.inject_fault(f='cdb_copy_start_after_dispatch', y='interrupt', seg_id=1)

        pwd = os.getcwd()
        PSQL.run_sql_command("""
          COPY mpp15834 TO '{0}/mpp15834.data';
        """.format(pwd))
