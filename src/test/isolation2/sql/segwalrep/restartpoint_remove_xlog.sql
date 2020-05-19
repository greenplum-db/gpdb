-- Test a bug that restartpoint removes xlog segment files which still
-- has prepared but not-yet-committed/aborted transactions.

include: helpers/server_helpers.sql;

!\retcode gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
!\retcode gpconfig -c wal_keep_segments -v 0 --skipvalidation;
-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery. Approximately we want to wait 30 seconds.
!\retcode gpconfig -c gp_gang_creation_retry_count -v 120 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 1000 --skipvalidation --masteronly;
!\retcode gpstop -u;

create extension if not exists gp_inject_fault;

create or replace function wait_for_replication_replay (retries int) returns bool as
$$
declare
	i int; /* in func */
	result bool; /* in func */
begin /* in func */
	i := 0; /* in func */
	-- Wait until the mirror (content 0) has replayed up to flush location
	loop /* in func */
		SELECT flush_location = replay_location INTO result from gp_stat_replication where gp_segment_id = 0; /* in func */
		if result then /* in func */
			return true; /* in func */
		end if; /* in func */

		if i >= retries then /* in func */
		   return false; /* in func */
		end if; /* in func */
		perform pg_sleep(0.1); /* in func */
		i := i + 1; /* in func */
	end loop; /* in func */
end; /* in func */
$$ language plpgsql;


create table t_restart (a int);

-- generate an orphaned prepare transaction.
select gp_inject_fault('dtm_broadcast_prepare', 'suspend', dbid)
  from gp_segment_configuration where role = 'p' and content = -1;
-- assume (2), (1) are on different segments and one tuple is on the first segment.
-- the test finally double-check that.
1&: insert into t_restart values(2),(1);
select gp_wait_until_triggered_fault('dtm_broadcast_prepare', 1, dbid)
  from gp_segment_configuration where role = 'p' and content = -1;

-- trigger xlog file switch on the first segment.
-- start_ignore
0U: select pg_switch_xlog();
-- end_ignore
checkpoint;
-- start_ignore
0U: select pg_switch_xlog();
-- end_ignore
checkpoint;

-- wait until the restartpoints on seg0 finish so that if the bug is not fixed,
-- the xlog segment file with the orphaned prepare transaction will be removed,
-- and then if the mirror is promoted it will panic like this:
-- FATAL","58P01","requested WAL segment pg_xlog/000000010000000000000003 has already been removed
-- The call stack is: StartupXLOG()->PrescanPreparedTransactions()...
select * from wait_for_replication_replay(5000);

-- shutdown primary and make sure the segment is down
-1U: select pg_ctl((SELECT datadir from gp_segment_configuration c
  where c.role='p' and c.content=0), 'stop', 'immediate');
select gp_request_fts_probe_scan();
select role, preferred_role from gp_segment_configuration where content = 0;

-- double confirm that promote succeeds.
-- also double confirm that
--  1. tuples (2) and (1) are located on two segments (thus we are testing 2pc with prepared transaction).
--  2. there are tuples on the first segment (we have been testing on the first segment).
insert into t_restart values(2),(1);
select gp_segment_id, * from t_restart;

select gp_inject_fault('dtm_broadcast_prepare', 'reset', dbid)
  from gp_segment_configuration where role = 'p' and content = -1;
1<:

-- confirm the "orphaned" prepared trnasaction commits finally.
select * from t_restart;

-- recovery the nodes
!\retcode gprecoverseg -a;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 2 from gp_segment_configuration where content = 0 and mode = 's') then /* in func */
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 2 from gp_segment_configuration where content = 0 and mode = 's') then /* in func */
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

-- verify the first segment is recovered to the original state.
select role, preferred_role from gp_segment_configuration where content = 0;

-- cleanup
drop table t_restart;
!\retcode gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation;
!\retcode gpconfig -r wal_keep_segments --skipvalidation;
!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation;
!\retcode gpstop -u;
