-- The goal of these tests is to validate that existing transactions on the
-- primary are not hung when the mirror gets promoted. In order to validate
-- this, mirror promotion is triggered at the following two-phase commit
-- points:
--   1) if the transaction hasn't prepared, then it should be aborted
--   2) if the transaction is already prepared, then it should complete commit
--      on the mirror
--   3) if the transaction has committed on the primary, but not acknowledged
--      to the master then it should complete the commit on the mirror

-- Test setup: This test needs a minimum of 3 primary/mirror pairs. In order to
-- minimize test time, each scenario is created on a different segment. Each
-- scenario fails over to the mirror and recovery for all 3 mirrors is done at the
-- end of the test.

-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/ERROR:  FTS detected connection lost during dispatch to seg0.*/
-- s/seg\d+.*/seg_DUMMY/gm
--
-- end_matchsubs

include: helpers/server_helpers.sql;

!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;
-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery. Approximately we want to wait 30 seconds.
!\retcode gpconfig -c gp_gang_creation_retry_count -v 127 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 250 --skipvalidation --masteronly;
!\retcode gpstop -u;
1:SELECT role, preferred_role, content FROM gp_segment_configuration;

-- Scenario 1: Not prepared
1:SELECT gp_inject_fault_infinite('start_prepare', 'infinite_loop', dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
1&:CREATE TABLE tolerance_test_table(an_int int);
2:SELECT gp_wait_until_triggered_fault('start_prepare', 1, dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
2:SELECT gp_inject_fault_infinite('fts_handle_message', 'error', dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
-- do fts probe request twice to guarantee the fault is triggered
2:SELECT gp_request_fts_probe_scan();
2:SELECT gp_request_fts_probe_scan();
1<:

1:SELECT gp_inject_fault('start_prepare', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
1:SELECT gp_inject_fault('fts_handle_message', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
1:SELECT role, preferred_role FROM gp_segment_configuration WHERE content = 0;
-- expect to fail with table not-exists
1:INSERT INTO tolerance_test_table VALUES(42);

-- Scenario 2: Prepared but not committed
-- NOTICE: Don't use session 2 again because it's cached gang is invalid
1:SELECT gp_inject_fault_infinite('transaction_abort_after_distributed_prepared', 'suspend', dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
1&:CREATE TABLE tolerance_test_table(an_int int);
3:SELECT gp_wait_until_triggered_fault('transaction_abort_after_distributed_prepared', 1, dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
3:SELECT gp_inject_fault_infinite('fts_handle_message', 'error', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
-- do fts probe request twice to guarantee the fault is triggered
3:SELECT gp_request_fts_probe_scan();
3:SELECT gp_request_fts_probe_scan();
3:SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'resume', dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
1<:

1:SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
1:SELECT gp_inject_fault('fts_handle_message', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
1:SELECT role, preferred_role FROM gp_segment_configuration WHERE content = 1;
1:INSERT INTO tolerance_test_table VALUES(42);

-- Scenario 3: Commit-Prepare received on primary but not acknowledged to master
-- NOTICE: Don't use session 2 again because it's cached gang is invalid
1:SELECT gp_inject_fault_infinite('finish_prepared_start_of_function', 'infinite_loop', dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
1&:DROP TABLE tolerance_test_table;
4:SELECT gp_wait_until_triggered_fault('finish_prepared_start_of_function', 1, dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
4:SELECT gp_inject_fault_infinite('fts_handle_message', 'error', dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
-- do fts probe request twice to guarantee the fault is triggered
4:SELECT gp_request_fts_probe_scan();
4:SELECT gp_request_fts_probe_scan();
1<:

-- Use new connection session. This helps is to make sure master is up and
-- running, even if in worst case the above Drop command commit-prepared retries
-- are exhausted and PANICs the master.
5:SELECT role, preferred_role FROM gp_segment_configuration WHERE content = 2;

5:!\retcode gprecoverseg -aF \-\-no-progress;

5:!\retcode gprecoverseg -ar;

!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation --masteronly;
!\retcode gpstop -u;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();
5:SELECT role, preferred_role, content FROM gp_segment_configuration;
