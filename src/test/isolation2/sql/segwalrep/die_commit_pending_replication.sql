-- Adding `2` as first column as the distribution column.
-- `2` should be on the first segment. let's double check here.
create table store_session_id(a int, sess_id int);
1: insert into store_session_id select 2, sess_id from pg_stat_activity where pid = pg_backend_pid();
1: select gp_segment_id, a from store_session_id;

1: create table die_commit_pending_replication(a int, b int);

-- Suspend to hit commit-prepared point on segment (as we are
-- interested in testing Commit here and not really Prepare)
select gp_inject_fault_infinite('finish_prepared_start_of_function', 'suspend', dbid) from gp_segment_configuration where role='p' and content = 0;
1&: insert into die_commit_pending_replication values(2),(1);
select gp_wait_until_triggered_fault('finish_prepared_start_of_function', 1, dbid) from gp_segment_configuration where role='p' and content = 0;

-- Now pause the wal sender on primary for content 0
select gp_inject_fault_infinite('wal_sender_loop', 'suspend', dbid) from gp_segment_configuration where role='p' and content = 0;

-- Insert fault in the ProcDiePending code block.
select gp_inject_fault_infinite('sync_rep_query_die', 'skip', dbid) from gp_segment_configuration where role='p' and content = 0;

-- Let the transaction move forward with the commit
select gp_inject_fault('finish_prepared_start_of_function', 'reset', dbid) from gp_segment_configuration where role='p' and content = 0;

-- Ensure the ProcDiePending code block is called. It implies 'SyncRep' in pg_stat_activity.
select gp_wait_until_triggered_fault('sync_rep_query_die', 1, dbid) from gp_segment_configuration where role='p' and content = 0;

-- We can terminate the backend on QE now.
0U: select pg_terminate_backend(pid) from pg_stat_activity
      where wait_event='SyncRep' and
      sess_id in (select sess_id from store_session_id);

-- We expect two more occurrence: one for backend quitting and another for retry.
select gp_wait_until_triggered_fault('sync_rep_query_die', 3, dbid) from gp_segment_configuration where role='p' and content = 0;

-- Verify that the sess_id changes due to retry.
0U: select pid,sess_id,wait_event,query from pg_stat_activity
      where sess_id in (select sess_id from store_session_id);

-- resume the primary wal replication so that retry could complete.
select gp_inject_fault('wal_sender_loop', 'reset', dbid) from gp_segment_configuration where role='p' and content = 0;

-- reset sync_rep_query_die
select gp_inject_fault('sync_rep_query_die', 'reset', dbid) from gp_segment_configuration where role='p' and content = 0;

1<:

-- check if the insert fails or not.
select gp_segment_id, * from die_commit_pending_replication;

--
-- Test 1PC: if there is transaction xlog commit on QD, and QE quits during the
-- "notification" stage, QD won't panic. Previously the notification code is
-- after xlog commit code on QD so QE quitting will cause notification on QD
-- elog(ERROR) but at that moment the transaction can not be aborted (caused by
-- elog(ERROR)), else panic with message as below, "cannot abort transaction
-- %u, it was already committed".
--

2: create temp table die_commit_pending_replication2(a int);
2: insert into die_commit_pending_replication2 values(2),(1);

-- Insert fault before the "notification" code on QE and susped there.
select gp_inject_fault_infinite('start_performDtxProtocolCommitOnePhase', 'error', dbid) from gp_segment_configuration where role='p' and content = 0;

-- Start an one phase query that writes commit xlog on QD.
2: begin; select txid_current(); select * from die_commit_pending_replication2; end;

-- Reset start_performDtxProtocolCommitOnePhase.
select gp_inject_fault_infinite('start_performDtxProtocolCommitOnePhase', 'reset', dbid) from gp_segment_configuration where role='p' and content = 0;

-- cleanup
drop table die_commit_pending_replication;
drop table store_session_id;
