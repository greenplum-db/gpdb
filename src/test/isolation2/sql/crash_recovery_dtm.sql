-- Test scenarios covering crash recovery for some specific DTM point.
--
-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/(PANIC):.*unable to complete*/
-- s/gid \=\s*\d+-\d+/gid \= DUMMY/gm
--
-- m/^ERROR:  Error on receive from seg0.*: server closed the connection unexpectedly/
-- s/^ERROR:  Error on receive from seg0.*: server closed the connection unexpectedly/ERROR: server closed the connection unexpectedly/
--
-- end_matchsubs

include: helpers/server_helpers.sql;

-- This function is used to loop until master shutsdown, to make sure
-- next command executed is only after restart and doesn't go through
-- while PANIC is still being processed by master, as master continues
-- to accept connections for a while despite undergoing PANIC.
CREATE OR REPLACE FUNCTION wait_till_master_shutsdown()
RETURNS void AS
$$
  DECLARE
    i int; /* in func */
  BEGIN
    i := 0; /* in func */
    while i < 120 loop
      i := i + 1; /* in func */
      PERFORM pg_sleep(.5); /* in func */
    end loop; /* in func */
  END; /* in func */
$$ LANGUAGE plpgsql;

1:SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
-- Scenario 1: Test to fail broadcasting of COMMIT PREPARED to one
-- segment and hence trigger PANIC in master while after completing
-- phase 2 of 2PC. Master's recovery cycle should correctly broadcast
-- COMMIT PREPARED again because master should find distributed commit
-- record in its xlog during recovery. Verify that the transaction is
-- committed after recovery. This scenario used to create cluster
-- inconsistency due to bug fixed now, as transaction used to get
-- committed on all segments except one where COMMIT PREPARED
-- broadcast failed before recovery. Master used to miss sending the
-- COMMIT PREPARED across restart and instead abort the transaction
-- after querying in-doubt prepared transactions from segments.
1: CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- Inject fault to fail the COMMIT PREPARED always on one segment, till fault is not reset
1: SELECT gp_inject_fault_infinite('finish_prepared_start_of_function', 'error', dbid)
   from gp_segment_configuration where content=0 and role='p';
-- Start looping in background, till master panics and closes the session
3&: SELECT wait_till_master_shutsdown();
-- Start transaction which should hit PANIC as COMMIT PREPARED will fail to one segment
1: CREATE TABLE commit_phase1_panic(a int, b int);
-- Reset the fault in utility mode because normal mode connection will
-- not be accepted until DTX recovery is finished.
-1U: SELECT gp_inject_fault('finish_prepared_start_of_function', 'reset', dbid)
     from gp_segment_configuration where content=0 and role='p';
-- Join back to know master has completed postmaster reset.
3<:
-- Start a session on master which would complete the DTM recovery and hence COMMIT PREPARED
4: SELECT * from commit_phase1_panic;
4: INSERT INTO commit_phase1_panic select i,i from generate_series(1, 10)i;
4: SELECT count(*) from commit_phase1_panic;

-- Scenario 2: Inject FATAL on master after recording commit but
-- before broadcasting COMMIT_PREPARED to segments. FATAL must convert
-- to PANIC and make sure to complete the 2PC processing and not leave
-- dangling prepared transaction. There used to bug as a result the
-- master backend process would just die, leaving dangling prepared
-- transaction on segment but commited on master.

-- Start looping in background, till master panics and closes the
-- session
5&: SELECT wait_till_master_shutsdown();
6: SELECT gp_inject_fault('dtm_broadcast_commit_prepared', 'fatal', dbid)
   from gp_segment_configuration where role='p' and content=-1;
6: CREATE TABLE commit_fatal_fault_test_table(a int, b int);
5<:
-- Start a session on master which would complete the DTM recovery and hence COMMIT PREPARED
7: SELECT count(*) from commit_fatal_fault_test_table;
7: SELECT * FROM gp_dist_random('pg_prepared_xacts');
7: SELECT gp_inject_fault('dtm_broadcast_commit_prepared', 'reset', dbid)
   from gp_segment_configuration where role='p' and content=-1;

-- Scenario 3: Inject ERROR after prepare phase has completed to
-- trigger abort. Then on abort inject FATAL on master before sending
-- ABORT_PREPARED. FATAL must convert to PANIC and make sure to
-- complete the 2PC processing and not leave dangling prepared
-- transaction. There used to bug as a result the master backend
-- process would just die, leaving dangling prepared transaction on
-- segment but aborted on master.

-- Start looping in background, till master panics and closes the
-- session
8&: SELECT wait_till_master_shutsdown();
9: SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'error', dbid)
   from gp_segment_configuration where role='p' and content=-1;
9: SELECT gp_inject_fault('dtm_broadcast_abort_prepared', 'fatal', dbid)
   from gp_segment_configuration where role='p' and content=-1;
9: CREATE TABLE abort_fatal_fault_test_table(a int, b int);
8<:
10: SELECT count(*) from abort_fatal_fault_test_table;
10: SELECT * FROM gp_dist_random('pg_prepared_xacts');
10: SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'reset', dbid)
    from gp_segment_configuration where role='p' and content=-1;
10: SELECT gp_inject_fault('dtm_broadcast_abort_prepared', 'reset', dbid)
    from gp_segment_configuration where role='p' and content=-1;

-- Scenario 4: QE panics after writing prepare xlog record. This
-- should cause master to broadcast abort and QEs handle the abort in
-- DTX_CONTEXT_LOCAL_ONLY context.
11: CREATE TABLE QE_panic_test_table(a int, b int);
11: INSERT INTO QE_panic_test_table SELECT * from generate_series(0, 9);
-- To help speedy recovery
11: CHECKPOINT;
-- Set to maximum number of 2PC retries to avoid any failures. Alter
-- system is required to set the GUC and can't be set on session level
-- as session reset happens for every abort retry.
11: alter system set dtx_phase2_retry_count to 1500;
11: select pg_reload_conf();
-- skip FTS probes always
11: SELECT gp_inject_fault_infinite('fts_probe', 'skip', dbid)
    from gp_segment_configuration where role='p' and content=-1;
11: SELECT gp_request_fts_probe_scan();
11: select gp_wait_until_triggered_fault('fts_probe', 1, dbid)
    from gp_segment_configuration where role='p' and content=-1;
11: SELECT gp_inject_fault('end_prepare_two_phase', 'infinite_loop', dbid)
    from gp_segment_configuration where role='p' and content=0;
-- statement to trigger fault after writing prepare record
12&: DELETE FROM QE_panic_test_table;
11: SELECT gp_wait_until_triggered_fault('end_prepare_two_phase', 1, dbid)
    from gp_segment_configuration where role='p' and content=0;
11: SELECT pg_ctl(datadir, 'restart') from gp_segment_configuration where role = 'p' and content = 0;
12<:
13: SELECT count(*) from QE_panic_test_table;
13: SELECT * FROM gp_dist_random('pg_prepared_xacts');
13: SELECT gp_inject_fault('fts_probe', 'reset', dbid)
    from gp_segment_configuration where role='p' and content=-1;
13: alter system reset dtx_phase2_retry_count;
13: select pg_reload_conf();
