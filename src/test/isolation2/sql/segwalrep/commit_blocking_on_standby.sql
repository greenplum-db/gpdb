--
-- Master/standby commit blocking scenarios.  These are different from
-- primary/mirror commit blocking because there is no FTS or a
-- third-party as external arbiter in case of master/standby.
--
-- Scenario1: Commits should block until standby confirms WAL flush
-- upto commit LSN.

-- Check that are starting with a clean slate, standby must be in sync
-- with master.
select application_name, state, sync_state from pg_stat_replication;

-- Inject fault on standby to skip WAL flush.
select gp_inject_fault_infinite('walrecv_skip_flush', 'skip', dbid)
from gp_segment_configuration where content=-1 and role='m';

begin;
create or replace function wait_for_pg_stat_activity(timeout_secs int)
returns void as $$
declare
  c int; /* in func */
  i int; /* in func */
begin
  c := 0; /* in func */
  i := 0; /* in func */
  while c < 1 and i < timeout_secs*2 loop
    select count(*) into c from pg_stat_activity
    where waiting_reason = 'replication'; /* in func */
    perform pg_sleep(0.5); /* in func */
    perform pg_stat_clear_snapshot(); /* in func */
    i := i + 1; /* in func */
  end loop; /* in func */
  if c < 1 then
    raise exception 'timeout waiting for command to get blocked'; /* in func */
  end if; /* in func */
end; /* in func */
$$ language plpgsql;

-- Flush WAL to trigger the fault on standby.
checkpoint;

select gp_wait_until_triggered_fault('walrecv_skip_flush', 1, dbid)
from gp_segment_configuration where content=-1 and role='m';

-- Should block in commit (SyncrepWaitForLSN()), waiting for commit
-- LSN to be flushed on standby.
1&: create table commit_blocking_on_standby_t1 (a int) distributed by (a);

-- The create table command should be seen as blocked.  Wait until
-- that happens.
select wait_for_pg_stat_activity(60);
select datname, waiting_reason, query from pg_stat_activity
where waiting_reason = 'replication';

select gp_inject_fault('walrecv_skip_flush', 'reset', dbid)
from gp_segment_configuration where content=-1 and role='m';

-- Ensure that commits are no longer blocked.
commit;

1<:

-- The blocked commit must have finished and the table should be ready
-- for insert.
insert into commit_blocking_on_standby_t1 values (1);


-- Scenario2: In CATCHUP phase, commits should not block until standby
-- has caught up within range.  And thereafter, commits should start
-- blocking.

-- In order to get master and standby in CATCHUP state, existing
-- connection, which is in STREAMING state must be closed.  A new
-- connection will then be initiated by standby, beginning in STARTUP
-- then CATCHUP to STREAMING.  Faults are used to suspend WAL sender
-- before entering STREAMING state.

-- Suspend WAL sender in main loop.  "infinite_loop" fault type does
-- not block signals.
select gp_inject_fault_infinite('wal_sender_loop', 'infinite_loop', dbid)
       from gp_segment_configuration where content=-1 and role='p';

-- Inject fault on standby to skip WAL flush.
select gp_inject_fault_infinite('walrecv_skip_flush', 'skip', dbid)
       from gp_segment_configuration where content=-1 and role='m';

-- Kill existing walsender.  WAL sender and WAL receiver processes
-- will be restarted and new connection will be established.  Note
-- that the faults injected are still in effect and will affect the
-- newly forked WAL sender and receiver processes.
select pg_terminate_backend(pid), sync_state from pg_stat_replication;

-- Should be set to 1 WAL segment by default.  Standby is considered
-- caught up if its flush_location is less than 1 WAL segment (64MB)
-- away from sent_location.
show repl_catchup_within_range;

-- Start a transaction, execute a DDL and commit.  The commit should
-- not block.
begin;

create or replace function wait_until_standby_in_state(targetstate text)
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

select wait_until_standby_in_state('catchup');

select gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid)
       from gp_segment_configuration where content=-1 and role='p';

-- WAL sender should be stuck in CATCHUP state.
select application_name, state, sync_state from pg_stat_replication;

-- This commit should NOT block because WAL sender has not yet
-- processed caughtup within range as it is stuck at the beginning of
-- main loop.
commit;

select gp_inject_fault('wal_sender_after_caughtup_within_range', 'suspend', dbid)
       from gp_segment_configuration where content=-1 and role='p';

select gp_inject_fault('wal_sender_loop', 'reset', dbid)
       from gp_segment_configuration where content=-1 and role='p';

-- Once this fault is triggered, WAL sender should have set
-- caughtup_within_range to true because difference between
-- sent_location and flush_location is within 1 WAL segment (64) MB.
select gp_wait_until_triggered_fault(
       'wal_sender_after_caughtup_within_range', 1, dbid)
       from gp_segment_configuration where content=-1 and role='p';

-- Should block because standby is considered to have caughtup within
-- range.
1&: create table commit_blocking_on_standby_t2 (a int) distributed by (a);

-- The create table command should be seen as blocked.
select wait_for_pg_stat_activity(60);
select datname, waiting_reason, query from pg_stat_activity
where waiting_reason = 'replication';

-- Reset faults on primary as well as mirror.
select gp_inject_fault('all', 'reset', dbid)
       from gp_segment_configuration where content=-1;

1<:

-- Create table transaction must have committed and the table should
-- be ready for insert.
insert into commit_blocking_on_standby_t2 values (1);

select wait_until_standby_in_state('streaming');
