-- Test to make sure FTS doesn't mark primary down if its recovering. Fault
-- 'fts_conn_startup_packet' is used to simulate the primary responding
-- in-recovery to FTS, primary is not actually going through crash-recovery in
-- test.
select role, preferred_role, mode, status from gp_segment_configuration where content = 0;
select gp_inject_fault_infinite('fts_conn_startup_packet', 'skip', dbid)
from gp_segment_configuration where content = 0 and role = 'p';
-- to make test deterministic and fast
-- start_ignore
\!gpconfig -c gp_fts_probe_retries -v 2 --masteronly
-- end_ignore

-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery. Approximately we want to wait 2 minutes at most.
-- start_ignore
\!gpconfig -c gp_gang_creation_retry_count -v 127 --skipvalidation --masteronly
\!gpconfig -c gp_gang_creation_retry_timer -v 1000 --skipvalidation --masteronly
\!gpstop -u
-- end_ignore
-- Wait a few seconds, to ensure the config changes take effect.
select pg_sleep(5);
show gp_fts_probe_retries;
select gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_conn_startup_packet', 3, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
select role, preferred_role, mode, status from gp_segment_configuration where content = 0;

-- test other scenario where recovery on primary is hung and hence FTS marks
-- primary down and promotes mirror. When 'fts_recovery_in_progress' is set to
-- skip it mimics the behavior of hung recovery on primary.
select gp_inject_fault_infinite('fts_recovery_in_progress', 'skip', dbid)
from gp_segment_configuration where content = 0 and role = 'p';
-- We call gp_request_fts_probe_scan twice to guarantee that the scan happens
-- after the fts_recovery_in_progress fault has been injected. If periodic fts
-- probe is running when the first request scan is run it is possible to not
-- see the effect due to the fault.
select gp_request_fts_probe_scan();
select gp_request_fts_probe_scan();
select role, preferred_role, mode, status from gp_segment_configuration where content = 0;

-- The remaining steps are to bring back the cluster to original state.
-- start_ignore

-- Wait until content 0 mirror is promoted otherwise, gprecoverseg
-- that runs after will fail.
do $$
declare
  y int;
begin
  for i in 1..120 loop
    begin
      select count(*) into y from gp_dist_random('gp_id');
      raise notice 'got % results, mirror must have been promoted', y;
      return;
    exception
      when others then
        raise notice 'mirror may not be promoted yet: %', sqlerrm;
        perform pg_sleep(0.5);
    end;
  end loop;
end;
$$;

\! gprecoverseg -av --no-progress
-- end_ignore

-- loop while segments come in sync
do $$
begin
  for i in 1..120 loop
    if (select count(*) = 0 from gp_segment_configuration where content = 0 and mode != 's') then
      return;
    end if;
    perform gp_request_fts_probe_scan();
  end loop;
end;
$$;
select role, preferred_role, mode, status from gp_segment_configuration where content = 0;

-- start_ignore
\! gprecoverseg -arv
-- end_ignore

-- loop while segments come in sync
do $$
begin
  for i in 1..120 loop
    if (select count(*) = 0 from gp_segment_configuration where content = 0 and mode != 's') then
      return;
    end if;
    perform gp_request_fts_probe_scan();
  end loop;
end;
$$;
select role, preferred_role, mode, status from gp_segment_configuration where content = 0;

-- start_ignore
\!gpconfig -r gp_fts_probe_retries --masteronly
\!gpconfig -r gp_gang_creation_retry_count --skipvalidation --masteronly
\!gpconfig -r gp_gang_creation_retry_timer --skipvalidation --masteronly
\!gpstop -u
-- end_ignore

-- cleanup steps
select gp_inject_fault('all', 'reset', dbid)
from gp_segment_configuration where content = 0 and role = 'p';
