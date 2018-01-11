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

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.filerep_util import Filerepe2e_Util
import tinctest
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.lib.gprecoverseg import GpRecover

import os
import time

class mpp23395(MPPTestCase):

    @classmethod
    def setUpClass(cls):
        super(mpp23395, cls).setUpClass()
        recoverseg = GpRecover()
        recoverseg.recover_rebalance_segs()

    def tearDown(self):
        self.util.inject_fault(f='dtm_broadcast_commit_prepared', y='reset', seg_id=1);
        self.util.inject_fault(f='dtm_broadcast_abort_prepared', y='reset', seg_id=1);
        self.util.inject_fault(f='transaction_abort_after_distributed_prepared', y='reset', seg_id=1);
        self.util.inject_fault(f='twophase_transaction_commit_prepared', y='reset', seg_id=2);
        super(mpp23395, self).tearDown()

    def check_no_dangling_prepared_transaction(self):
        """
        Check if pg_prepared_xacts reports any records.
        """
	while True:
            sql = "SELECT count(*) FROM pg_prepared_xacts"
            # Use -A and -t, suppress -a, to get only the number.
            psql = PSQL(sql_cmd=sql, flags='-A -t')
            psql.run()
            results = psql.get_results()
            if psql.get_results().rc == 0:
                break

	if (results.stdout.strip() != '0'):
           PSQL.run_sql_command("""
          SELECT * FROM gp_dist_random('pg_prepared_xacts');
          """)

        # Should be zero.
        self.assertEqual(results.stdout.strip(), '0')

    def run_sequence(self, sql, fault, fault_type, segid, should_panic=True):
        (ok,out) = self.util.inject_fault(f=fault, y=fault_type, seg_id=segid);
        if not ok:
           raise Exception("Fault dtm_broadcast_commit_prepared injection failed")
        psql = PSQL(sql_cmd=sql);
        tinctest.logger.debug("Executing:" + sql)
        psql.run()
        results = psql.get_results()
        tinctest.logger.debug(results.stderr)

        self.check_no_dangling_prepared_transaction()

	if "PANIC" not in results.stderr and should_panic:
            raise Exception("Fault %s type %s (on segid: %d) did not cause the master reset" % (fault, fault_type, segid))

	if "PANIC" in results.stderr and not should_panic:
            raise Exception("Fault %s type %s (on segid: %d) caused a PANIC. dtx two phase retry failed" % (fault, fault_type, segid))

        PSQL.wait_for_database_up()

    def test_mpp23395(self):
        """
        
        @description Test MPP-20964, uncleaned lock table by pg_terminate_backend
        @product_version gpdb: [4.3.3.1-],[4.2.8.5-4.2.99.99]
        """
        self.util = Filerepe2e_Util()

        (ok,out) = self.util.inject_fault(f='dtm_broadcast_commit_prepared', y='reset', seg_id=1);
        if not ok:
           raise Exception("Failed to reset the fault dtm_broadcast_commit_prepared")
 
        # setup
        PSQL.run_sql_command("""
          DROP TABLE IF EXISTS mpp23395;
          """)

        # Scenario 1: FAULT during Create Table on master
        sql = '''
        CREATE TABLE mpp23395(a int);
        '''
        self.run_sequence(sql, 'dtm_broadcast_commit_prepared', 'fatal', 1);

        # Scenario 2: FAULT during Drop Table on master, COMMIT case
        sql = '''
        DROP TABLE mpp23395;
        '''
        self.run_sequence(sql, 'dtm_broadcast_commit_prepared', 'fatal', 1);

        (ok,out) = self.util.inject_fault(f='dtm_broadcast_commit_prepared', y='reset', seg_id=1);
        if not ok:
           raise Exception("Failed to reset the fault dtm_broadcast_commit_prepared")

        # Scenario 3: FAULT during Create Table on segment, COMMIT case
        sql = '''
        SET dtx_phase2_retry_count = 1;
        SET debug_dtm_action_target = "protocol";
        SET debug_dtm_action_protocol = "commit_prepared";
        SET debug_dtm_action_segment = 0;
        SET debug_dtm_action = "fail_begin_command";
        CREATE TABLE mpp23395(a int);
        '''
        self.run_sequence(sql, 'twophase_transaction_commit_prepared', 'error', 2);

        # Scenario 4: FAULT during Drop Table on segment, COMMIT case
        sql = '''
        SET dtx_phase2_retry_count = 1;
        SET debug_dtm_action_target = "protocol";
        SET debug_dtm_action_protocol = "commit_prepared";
        SET debug_dtm_action_segment = 0;
        SET debug_dtm_action = "fail_begin_command";
        DROP TABLE mpp23395;
        '''
        self.run_sequence(sql, 'twophase_transaction_commit_prepared', 'error', 2);

        # Scenario 5: FAULT during Create Table on master, ABORT case
        (ok,out) = self.util.inject_fault(f='transaction_abort_after_distributed_prepared', y='error', seg_id=1);
        if not ok:
           raise Exception("Failed to set the error fault for transaction_abort_after_distributed_prepared")
 
        sql = '''
        CREATE TABLE mpp23395(a int);
        '''
        self.run_sequence(sql, 'dtm_broadcast_abort_prepared', 'fatal', 1);

        (ok,out) = self.util.inject_fault(f='transaction_abort_after_distributed_prepared', y='reset', seg_id=1);
        if not ok:
           raise Exception("Failed to reset the fault transaction_abort_after_distributed_prepared")


        PSQL.run_sql_command("""
        CREATE TABLE mpp23395(a int);
          """)

        # Scenario 6: FAULT during Drop Table on master, ABORT case
        (ok,out) = self.util.inject_fault(f='transaction_abort_after_distributed_prepared', y='error', seg_id=1);
        if not ok:
           raise Exception("Failed to set the error fault for transaction_abort_after_distributed_prepared")
 
        sql = '''
        DROP TABLE mpp23395;
        '''
        self.run_sequence(sql, 'dtm_broadcast_abort_prepared', 'fatal', 1);

        (ok,out) = self.util.inject_fault(f='transaction_abort_after_distributed_prepared', y='reset', seg_id=1);
        if not ok:
           raise Exception("Failed to reset the fault transaction_abort_after_distributed_prepared")

        PSQL.run_sql_command("""
        DROP TABLE mpp23395;
          """)


        # Scenario 7: FAULT during Create Table on segment, COMMIT case, succeeds on second retry
        sql = '''
        DROP TABLE IF EXISTS mpp23395;
        SET debug_dtm_action_target = "protocol";
        SET debug_dtm_action_protocol = "commit_prepared";
        SET debug_dtm_action_segment = 0;
        SET debug_dtm_action = "fail_begin_command";
        CREATE TABLE mpp23395(a int);
        '''
        self.run_sequence(sql, 'finish_prepared_after_record_commit_prepared', 'error', 2, False);

        # Scenario 8: QE panics after writing prepare xlog record.  This should
        # cause master to broadcast abort but QEs handle the abort in
        # DTX_CONTEXT_LOCAL_ONLY context.
        sql = '''
        DROP TABLE IF EXISTS mpp23395;
        CREATE TABLE mpp23395(a int);
        INSERT INTO mpp23395 VALUES(1), (2), (3);
        SET debug_abort_after_segment_prepared = true;
        DELETE FROM mpp23395;
        '''

        # No prepared transactions should remain lingering
        PSQL.run_sql_command(sql)
        self.check_no_dangling_prepared_transaction()

        dbstate = DbStateClass('run_validation')
        dbstate.check_catalog()
