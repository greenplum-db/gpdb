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

from mpp.lib.gpdbverify import GpdbVerify
from mpp.lib.PSQL import PSQL, PSQLException
from mpp.models import MPPTestCase
from gppylib.db import dbconn
from gppylib.gparray import GpArray
from tinctest import logger
from tinctest.lib import Gpdiff, local_path

# Instead of deriving from MPPTestCase, we could have used
# SQLIsolationTest and life would have been much easier.  The only
# problem with SQLIsolationTest is one needs to write a segment's port
# number in SQL file.  We cannot make any assumption on segment port
# numbers in the test at this time.  Hence this contrived way.
class AuxOidsTest(MPPTestCase):
    """
    @product_version gpdb: [4.3.5.1 - main]
    """
    def setUp(self):
        super(AuxOidsTest, self).setUp()
        self.gparray = GpArray.initFromCatalog(dbconn.DbURL(), utility=True)

    def test_pg_inherits(self):
        """
        Change order of children in pg_inherits on segments.  Alter should not
        cause inconsistent OIDs.

        """
        # Create paritioned table.
        sql = local_path("create_part_table.sql")
        out = local_path("create_part_table.out")
        ans = local_path("create_part_table.ans")
        PSQL.run_sql_file(sql, out)
        assert Gpdiff.are_files_equal(out, ans)

        # Change order of children in pg_inherits on segments but not
        # on master.
        sql = local_path("reorder_pg_inherits.sql")
        out = local_path("reorder_pg_inherits.out")
        ans = local_path("reorder_pg_inherits.ans")
        segments = [seg for seg in self.gparray.getSegDbList()
                    if seg.role == "p"]
        assert len(segments) > 0, "No primary segments found."
        primary = segments[0]
        PSQL.run_sql_file(
            sql, out, host=primary.hostname, port=primary.port,
            PGOPTIONS=("-c allow_system_table_mods=dml "
                       "-c gp_session_role=utility"))
        assert Gpdiff.are_files_equal(out, ans)

        # Alter the partitioned table so that it's rewritten.
        with dbconn.connect(dbconn.DbURL()) as conn:
            dbconn.execSQL(conn, "ALTER TABLE co1 ALTER COLUMN c2 TYPE int8")
            conn.commit()

        # Run gpcheckcat
        result = GpdbVerify().gpcheckcat(testname="inconsistent")
        # Test return code
        if result[0] != 0:
            logger.error(result[2]) # log output
            self.fail("gpcheckcat 'inconsistent' test failed")
