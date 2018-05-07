-- Test to make sure FTS doesn't mark primary down if its recovering. Fault
-- 'fts_conn_startup_packet' is used to simulate the primary responding
-- in-recovery to FTS, primary is not actually going through crash-recovery in
-- test.
create extension if not exists gp_inject_fault;
select role, preferred_role, mode from gp_segment_configuration where content = 0;
select gp_inject_fault('fts_conn_startup_packet', 'skip', '', '', '', -1, 0, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
-- to make test deterministic and fast
-- start_ignore
\!gpconfig -c gp_fts_probe_retries -v 2 --masteronly
\!gpstop -u
-- end_ignore
show gp_fts_probe_retries;
select gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_conn_startup_packet', 3, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
select role, preferred_role, mode from gp_segment_configuration where content = 0;

-- test other scenario where recovery on primary is hung and hence FTS marks
-- primary down and promotes mirror. When 'fts_recovery_in_progress' is set to
-- skip it mimics the behavior of hung recovery on primary.
select gp_inject_fault('fts_recovery_in_progress', 'skip', '', '', '', -1, 0, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
-- We call gp_request_fts_probe_scan twice to guarantee that the scan happens
-- after the fts_recovery_in_progress fault has been injected. If periodic fts
-- probe is running when the first request scan is run it is possible to not
-- see the effect due to the fault.
select gp_request_fts_probe_scan();
select gp_request_fts_probe_scan();
select role, preferred_role, mode from gp_segment_configuration where content = 0;

-- The remaining steps are to bring back the cluster to original state.
-- start_ignore
\! gprecoverseg -aF
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
select role, preferred_role, mode from gp_segment_configuration where content = 0;

-- start_ignore
\! gprecoverseg -ar
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
select role, preferred_role, mode from gp_segment_configuration where content = 0;

-- start_ignore
\!gpconfig -r gp_fts_probe_retries --masteronly
\!gpstop -u
-- end_ignore

-- cleanup steps
select gp_inject_fault('fts_recovery_in_progress', 'reset', '', '', '', -1, 0, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
select gp_inject_fault('fts_conn_startup_packet', 'reset', '', '', '', -1, 0, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
