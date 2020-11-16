-- Test error after ProcArrayEndTransaction

CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- abort fail on QD
SELECT gp_inject_fault_new( 'abort_after_procarray_end', 'error', 1);
BEGIN;
CREATE TABLE test_xact_abort_failure(a int);
ABORT;
SELECT gp_inject_fault_new( 'abort_after_procarray_end', 'reset', 1);

-- abort fail on QE
SELECT gp_inject_fault_new( 'abort_after_procarray_end', 'error', dbid) from gp_segment_configuration where role = 'p' and content = 0;
BEGIN;
CREATE TABLE test_xact_abort_failure(a int);
ABORT;
SELECT gp_inject_fault_new( 'abort_after_procarray_end', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;

-- abort fail in local transaction
SELECT gp_inject_fault_new( 'abort_after_procarray_end', 'error', dbid) from gp_segment_configuration where role = 'p' and content = 0;
2U: BEGIN;
2U: CREATE TABLE test_xact_abort_failure(a int);
2U: ABORT;
SELECT gp_inject_fault_new( 'abort_after_procarray_end', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;

-- check catalog tuple visibility while waiting for second phase of 2PC
SELECT gp_inject_fault_new( 'dtm_broadcast_commit_prepared', 'suspend', 1);
1&:CREATE TABLE test_qd_visibility(a int);
SELECT gp_wait_until_triggered_fault( 'dtm_broadcast_commit_prepared', 1, 1);
-- confirm the transaction is committed in clog
SET gp_select_invisible to ON;
SELECT status FROM gp_transaction_log WHERE transaction IN (SELECT xmin FROM pg_class WHERE relname = 'test_qd_visibility');
SET gp_select_invisible to OFF;
-- the tuple should not be visible
SELECT relname FROM pg_class WHERE relname = 'test_qd_visibility';
SELECT gp_inject_fault_new( 'dtm_broadcast_commit_prepared', 'reset', 1);
1<:
-- now the tuple should be visible
SELECT relname FROM pg_class WHERE relname = 'test_qd_visibility';
