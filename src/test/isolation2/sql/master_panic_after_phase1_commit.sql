-- Test to fail broadcasting of COMMIT PREPARED to one segment and hence trigger
-- PANIC in master while after completing phase 2 of 2PC. Master's recovery
-- cycle should correctly broadcast COMMIT PREPARED again because master should
-- find distributed commit record in its xlog during recovery. Verify that the
-- transaction is committed after recovery. This scenario used to create cluster
-- inconsistency due to bug fixed now, as transaction used to get committed on
-- all segments except one where COMMIT PREPARED broadcast failed before
-- recovery. Master used to miss sending the COMMIT PREPARED across restart and
-- instead abort the transaction after querying in-doubt prepared transactions
-- from segments.

-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/(PANIC):.*unable to complete*/
-- s/gid \=\s*\d+-\d+/gid \= DUMMY/gm
--
-- end_matchsubs

-- This function is used to loop until master shutsdown, to make sure next
-- command executed is only after restart and doesn't go through while PANIC is
-- still being processed by master, as master continues to accept connections
-- for a while despite undergoing PANIC.
CREATE OR REPLACE FUNCTION wait_till_master_shutsdown()
RETURNS void AS
$$
  BEGIN
    loop
      PERFORM pg_sleep(.5); /* in func */
    end loop; /* in func */
  END; /* in func */
$$ LANGUAGE plpgsql;

1: CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- Inject fault to fail the COMMIT PREPARED always on one segment, till fault is not reset
1: SELECT gp_inject_fault('finish_prepared_start_of_function', 'error', '', '', '', -1, 0, 2);
-- create utility session to segment which will be used to reset the fault
0U: SELECT 1;
-- Start looping in background, till master panics and closes the session
3&: SELECT wait_till_master_shutsdown();
-- Start transaction which should hit PANIC as COMMIT PREPARED will fail to one segment
1: CREATE TABLE commit_phase1_panic(a int, b int);
-- Reset the fault using utility mode connection
0U: SELECT gp_inject_fault('finish_prepared_start_of_function', 'reset', 2);
-- Join back to know master has completed postmaster reset.
3<:
-- Start a session on master which would complete the DTM recovery and hence COMMIT PREPARED
4: SELECT * from commit_phase1_panic;
4: INSERT INTO commit_phase1_panic select i,i from generate_series(1, 10)i;
4: SELECT count(*) from commit_phase1_panic;
