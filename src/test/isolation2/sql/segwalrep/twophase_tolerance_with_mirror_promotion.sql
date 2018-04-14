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

CREATE extension IF NOT EXISTS gp_inject_fault;
1:set dtx_phase2_retry_count=5;
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;
!\retcode gpstop -u;
1:SELECT role, preferred_role, content FROM gp_segment_configuration;

-- Scenario 1: Not prepared
1:SELECT gp_inject_fault('start_prepare', 'infinite_loop', dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
1&:CREATE TABLE tolerance_test_table(an_int int);
2:SELECT gp_inject_fault('fts_handle_message', 'error', '', '', '', -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 0 AND role = 'p';
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
1:SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'suspend', dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
1&:CREATE TABLE tolerance_test_table(an_int int);
2:SELECT gp_inject_fault('fts_handle_message', 'error', '', '', '', -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
2:SELECT gp_request_fts_probe_scan();
2:SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'resume', dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
1<:

1:SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
1:SELECT gp_inject_fault('fts_handle_message', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 1 AND role = 'p';
1:SELECT role, preferred_role FROM gp_segment_configuration WHERE content = 1;
1:INSERT INTO tolerance_test_table VALUES(42);

-- Scenario 3: Commit-Prepare received on primary but not acknowledged to master
1:SELECT gp_inject_fault('finish_prepared_start_of_function', 'infinite_loop', dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
1&:DROP TABLE tolerance_test_table;
2:SELECT gp_inject_fault('fts_handle_message', 'error', '', '', '', -1, 0, dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
2:SELECT gp_request_fts_probe_scan();
1<:

1:SELECT gp_inject_fault('finish_prepared_start_of_function', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
1:SELECT gp_inject_fault('fts_handle_message', 'reset', dbid)
  FROM gp_segment_configuration WHERE content = 2 AND role = 'p';
1:SELECT role, preferred_role FROM gp_segment_configuration WHERE content = 2;

1:!\retcode gprecoverseg -aF;
1:!\retcode gprecoverseg -ar;
!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpstop -u;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 0 from gp_segment_configuration where content != -1 and mode != 's') then /* in func */
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;
1:SELECT role, preferred_role, content FROM gp_segment_configuration;
