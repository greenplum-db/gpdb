create extension if not exists gp_inject_fault;
select count(*) = 2 as in_sync from gp_segment_configuration
where content = 0 and mode = 's';
-- Once this fault is hit, FTS process should abort current
-- transaction and exit.
select gp_inject_fault('fts_update_config', 'error', '', '', '', -1, 0, 1);
-- FTS probe connection should encounter an error due to this fault,
-- injected on content 0 primary.
select gp_inject_fault('fts_handle_message', 'error', '', '', '', -1, 0, dbid)
from gp_segment_configuration where content = 0 and role = 'p';
-- Upon failure to probe content 0 primary, FTS will try to update the
-- configuration.  The update to configuration will hit error due to
-- the "fts_update_config" fault.
select gp_inject_fault('fts_update_config', 'wait_until_triggered', 1);

select gp_inject_fault('fts_handle_message', 'reset', dbid)
from gp_segment_configuration where content = 0 and role = 'p';

select gp_inject_fault('fts_update_config', 'reset', 1);

-- Verify that FTS didn't leak any locks due to the error during
-- config update.
select locktype, mode, relation, pid, granted from pg_locks where
relation = 'gp_segment_configuration'::regclass or
relation = 'gp_configuration_history'::regclass;
select gp_inject_fault('fts_update_config', 'reset', 1);
-- Postmaster should have restarted FTS by now.  Trigger a scan and
-- validate that configuration is sane.
select gp_request_fts_probe_scan();
select count(*) = 2 as in_sync from gp_segment_configuration
where content = 0 and mode = 's';
