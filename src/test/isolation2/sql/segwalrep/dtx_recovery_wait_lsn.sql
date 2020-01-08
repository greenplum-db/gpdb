-- Test this scenario:
-- mirror has latency replaying the WAL from the primary, the master is reset
-- from PANIC, master will start the DTX recovery process to recover the
-- in-progress two-phase transactions. When DTX recovery process performing
-- 'RECOVERY COMMIT PREPARED' on a transaction that is currently waiting for
-- sync replication, it should wait rather than error out.
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
1&: insert into t_wait_lsn select generate_series(1,10);
2<:

-- let walreceiver on mirror 0 skip WAL flush
2: select gp_inject_fault_infinite('walrecv_skip_flush', 'skip', dbid) from gp_segment_configuration where content=0 and role='m';
-- resume 'COMMIT PREPARED', session 1 will hang on 'SyncRepWaitForLSN'
2: select gp_inject_fault_infinite('finish_prepared_start_of_function', 'reset', dbid) from gp_segment_configuration where content=0 and role='p';

0U: select count(*) from pg_prepared_xacts;

-- trigger master reset
3: select gp_inject_fault('before_read_command', 'panic', 1);
3: select 1;

-- wait for master finish crash recovery
-1U: select wait_until_standby_in_state('streaming');

-- resume walreceiver on mirror 0
-1U: select gp_inject_fault_infinite('walrecv_skip_flush', 'reset', dbid) from gp_segment_configuration where content=0 and role='m';
-1Uq:

4: select count(*) from t_wait_lsn;
4: drop table t_wait_lsn;
4q:
