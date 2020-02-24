include: helpers/server_helpers.sql;

-- Test this scenario:
-- mirror has latency replaying the WAL from the primary, the master is reset
-- from PANIC, master will start the DTX recovery process to recover the
-- in-progress two-phase transactions. 
-- The FTS process should be able to continue probe and 'sync off' the mirror
-- while the 'dtx recovery' process is hanging recovering distributed transactions.
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;

1: create or replace function wait_until_standby_in_state(targetstate text)
returns void as $$
declare
   replstate text; /* in func */
begin
   loop
      select state into replstate from pg_stat_replication; /* in func */
      exit when replstate = targetstate; /* in func */
      perform pg_sleep(0.1); /* in func */
   end loop; /* in func */
end; /* in func */
$$ language plpgsql;

1: create table t_wait_lsn(a int);

-- suspend segment 0 before performing 'COMMIT PREPARED'
2: select gp_inject_fault_infinite('finish_prepared_start_of_function', 'suspend', dbid) from gp_segment_configuration where content=0 and role='p';
2&: select gp_wait_until_triggered_fault('finish_prepared_start_of_function', 1, dbid) from gp_segment_configuration where content=0 and role='p';
1&: insert into t_wait_lsn select generate_series(1,12);
2<:

-- let walreceiver on mirror 0 skip WAL flush
2: select gp_inject_fault_infinite('walrecv_skip_flush', 'skip', dbid) from gp_segment_configuration where content=0 and role='m';
-- resume 'COMMIT PREPARED', session 1 will hang on 'SyncRepWaitForLSN'
2: select gp_inject_fault_infinite('finish_prepared_start_of_function', 'reset', dbid) from gp_segment_configuration where content=0 and role='p';

0U: select count(*) from pg_prepared_xacts;

-- stop mirror
3: SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE content=0 AND role = 'm';
-- trigger master reset
3: select gp_inject_fault('before_read_command', 'panic', 1);
3: select 1;

-- wait for master finish crash recovery
-1U: select wait_until_standby_in_state('streaming');

-- wait for FTS to 'sync off' the mirror, meanwhile, dtx recovery process will restart repeatedly 
4: select count(*) from t_wait_lsn;

!\retcode gprecoverseg -a;
-- loop while segments come in sync
4: do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 2 from gp_segment_configuration where content in (0, 1) and mode = 's' and role = 'p') then /* in func */ 
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;

4: select count(*) from t_wait_lsn;
4: drop table t_wait_lsn;
4q:
