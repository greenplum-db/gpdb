select gp_inject_fault('sync_rep_query_cancel', 'reset', 2);
select gp_inject_fault('wal_sender_loop', 'reset', 2);

create or replace function wait_for_replication(iterations int)
returns bool as $$
begin /* in func */
	for i in 1 .. iterations loop /* in func */
	    if exists (select wait_event_type from pg_stat_activity where sess_id in (select sess_id from store_session_id) and wait_event_type = 'Replication') then /* in func */
	       return true; /* in func */
	    end if; /* in func */
	    perform pg_sleep(0.1); /* in func */
	    perform pg_stat_clear_snapshot(); /* in func */
	end loop; /* in func */
	return false; /* in func */
end; /* in func */
$$ language plpgsql VOLATILE;

SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
create table store_session_id(a int, sess_id int);
-- adding `2` as first column as the distribution column and add this tuple to segment 0
1: insert into store_session_id select 2, sess_id from pg_stat_activity where pid = pg_backend_pid();
-- suspend to hit commit-prepared point on segment (as we are
-- interested in testing Commit here and not really Prepare)
select gp_inject_fault_infinite('finish_prepared_start_of_function', 'suspend', 2);
-- Expect: `create table` should be blocked until reset
-- `wal_sender_loop`. We also verify the `sync_rep_query_cancel` is
-- triggered by query cancel request.
1&: create table cancel_commit_pending_replication(a int, b int);
select gp_wait_until_triggered_fault('finish_prepared_start_of_function', 1, 2);
-- now pause the wal sender on primary for content 0
select gp_inject_fault_infinite('wal_sender_loop', 'suspend', 2);
-- let the transaction move forward with the commit
select gp_inject_fault('finish_prepared_start_of_function', 'reset', 2);
-- loop to reach wait_event_type=Replication
0U: select wait_for_replication(1200);
-- hitting this fault, is checked for test validation
select gp_inject_fault_infinite('sync_rep_query_cancel', 'skip', 2);
0U: select pg_cancel_backend(pid) from pg_stat_activity where wait_event_type='Replication' and sess_id in (select sess_id from store_session_id);
-- EXPECT: hit this fault for QueryCancelPending
select gp_wait_until_triggered_fault('sync_rep_query_cancel', 1, 2);
-- EXPECT: the query is still in waiting mode, to verify the cancel is ignored.
0U: select wait_event_type from pg_stat_activity where sess_id in (select sess_id from store_session_id);
-- resume the primary on content 0
select gp_inject_fault('wal_sender_loop', 'reset', 2);
1<:
-- cleanup
select gp_inject_fault('sync_rep_query_cancel', 'reset', 2);
