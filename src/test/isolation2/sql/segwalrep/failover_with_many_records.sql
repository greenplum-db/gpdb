include: helpers/server_helpers.sql;
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Allow extra time for mirror promotion to complete recovery to avoid
-- gprecoverseg BEGIN failures due to gang creation failure as some primaries
-- are not up. Setting these increase the number of retries in gang creation in
-- case segment is in recovery. Approximately we want to wait 30 seconds.
!\retcode gpconfig -c gp_gang_creation_retry_count -v 120 --skipvalidation --masteronly;
!\retcode gpconfig -c gp_gang_creation_retry_timer -v 1000 --skipvalidation --masteronly;
!\retcode gpstop -u;

1:CREATE TABLE t(a int, b int);

1:SELECT gp_inject_fault_infinite('checkpoint_after_redo_calculated', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
2&:CHECKPOINT;
3:INSERT INTO t VALUES (1, 0);

-- Force WAL to switch xlog files explicitly
-- start_ignore
1U:SELECT pg_switch_xlog();
-- end_ignore
3:INSERT INTO t SELECT 0, i FROM generate_series(1, 25)i;

1:SELECT gp_inject_fault_infinite('checkpoint_after_redo_calculated', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
2<:

-- Here we must force immediately shutdown primary without triggering another
-- checkpoint otherwise we would undo all of our previous work to ensure that
-- checkpoint record has redo record on another wal segment.
-1U: SELECT pg_ctl((SELECT datadir FROM gp_segment_configuration c WHERE c.role='p' AND c.content=1), 'stop', 'immediate');

-- Make sure we see the segment is down before trying to recover...
4:SELECT gp_request_fts_probe_scan();
4:SELECT role, preferred_role FROM gp_segment_configuration WHERE content = 1;

!\retcode gprecoverseg -a;

-- loop while segments come in sync
do $$
begin /* in func */
  for i in 1..120 loop /* in func */
    if (select count(*) = 2 from gp_segment_configuration where content = 1 and mode = 's') then /* in func */
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
    if (select count(*) = 2 from gp_segment_configuration where content = 1 and mode = 's') then /* in func */
      return; /* in func */
    end if; /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
  end loop; /* in func */
end; /* in func */
$$;

-- verify no segment is down after recovery
1:SELECT COUNT(*) FROM gp_segment_configuration WHERE status = 'd';

!\retcode gpconfig -r gp_gang_creation_retry_count --skipvalidation --masteronly;
!\retcode gpconfig -r gp_gang_creation_retry_timer --skipvalidation --masteronly;
!\retcode gpstop -u;
