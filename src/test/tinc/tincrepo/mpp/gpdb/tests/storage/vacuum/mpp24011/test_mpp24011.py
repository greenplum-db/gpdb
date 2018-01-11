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

import time
from mpp.models import MPPTestCase
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.PSQL import PSQL

class test_mpp24011(MPPTestCase):

    '''
    
    @tags mpp-24011
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.2.8.1-4.2], [4.3.3-]
    '''

    def setUp(self):
        self.filereputil = Filerepe2e_Util()

    def tearDown(self):
        self.filereputil.inject_fault(f='repair_frag_end', y='reset', seg_id=2)

    def test_mpp24011(self):
        # setup
        PSQL.run_sql_command("""
            drop table if exists mpp24011;
            create table mpp24011(
            a int,
            b int,
            c text
            ) distributed by (a);

            insert into mpp24011
            select 1, i, repeat('x', 255-42) from generate_series(1, 32 * 4 * 3)i;

            create unique index impp24011 on mpp24011 (a, b);
            delete from mpp24011 where b between 1 and 127 or b between 128 and 253 or b between 382 and 383;
            checkpoint;
            update mpp24011 set b = b where b = 384;
        """)

        self.filereputil.inject_fault(f='repair_frag_end', y='reset', seg_id=2)
        self.filereputil.inject_fault(f='repair_frag_end', y='panic', seg_id=2)

        PSQL.run_sql_command("""
          VACUUM FULL FREEZE mpp24011;
        """)

        # wait until segment comes up
        success = False
        for i in range(60):
            res = PSQL.run_sql_command("""
                SELECT count(*) FROM mpp24011;
            """, flags='-t -A')
            if res is not None and res.strip() == "129":
                success = True
                break
            time.sleep(1)

        self.assertTrue(success, "check segment coming up after crash recovery")
