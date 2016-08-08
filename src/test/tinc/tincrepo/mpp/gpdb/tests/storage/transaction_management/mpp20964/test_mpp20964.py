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

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL

import os
import time

class test_mpp20964(MPPTestCase):

    def check_lock_corruption(self):
        """
        Check if pg_locks has records with transaction = 0, which is corrupted.
        """
        sql = "SELECT count(*) FROM pg_locks WHERE transaction = 0"
        # Use -A and -t, suppress -a, to get only the number.
        psql = PSQL(sql_cmd=sql, flags='-A -t')
        psql.run()
        results = psql.get_results()
        # Should be zero.
        self.assertEqual(results.stdout.strip(), '0')

    def test_mpp20964(self):
        """
        
        @description Test MPP-20964, uncleaned lock table by pg_terminate_backend
        @created 2013-08-21 00:00:00
        @modified 2013-08-21 00:00:00
        @tags mpp-20964
        @product_version gpdb: [4.2.6.3- main]
        """

        # setup
        PSQL.run_sql_command("""
          DROP TABLE IF EXISTS foo;
          CREATE TABLE foo AS SELECT i a, i b FROM generate_series(1, 100)i;
          """)

        # Let the backend run executor in QE and hang for a while.
        sql = ("""
          SELECT * FROM (
            SELECT count(CASE WHEN pg_sleep(1) IS NULL THEN 1 END)
            FROM foo
          )s;
        """)
        # Run it backend.  We'll kill him while he's running.
        PSQL(sql_cmd=sql, background=True).run(validateAfter=False)
        # Let him reach to QE, just in case.
        time.sleep(1)

        # Now I'm killing him.  Note we kill both QE and QD.  QD runs
        # proc_exit and tries to get result from running QE.  Since
        # QE also died due to the termination, QD tries to issue an ERROR,
        # turning it to FATAL, calling proc_exit again.  He loses a chance
        # to release locks.
        sql = """
          SELECT pg_terminate_backend(procpid)
          FROM(
            SELECT (pg_stat_get_activity(NULL)).*
            FROM gp_dist_random('gp_id')
            WHERE gp_segment_id = 0
          )s
          WHERE sess_id <> current_setting('gp_session_id')
          UNION
          SELECT pg_terminate_backend(procpid)
          FROM pg_stat_activity
          WHERE sess_id <> current_setting('gp_session_id')
        """
        PSQL.run_sql_command(sql)

        # Finally check the pg_locks view to check if corrupted records
        # are seen.  If proclock was left without valid PGPROC, it would show
        # transaction = 0.  For some reason, one time might pass the test.
        # Check five times.
        for i in range(5):
            self.check_lock_corruption()
