-- start_ignore
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
-- end_ignore

-- set these values purely to cut down test time, as default fts trigger is
-- every min and 5 retries
alter system set gp_fts_probe_interval to 10;
alter system set gp_fts_probe_retries to 1;
select pg_reload_conf();

select count(*) = 2 as in_sync from gp_segment_configuration
where content = 0 and mode = 's';

-- Once this fault is hit, FTS process should abort current
-- transaction and exit.
select gp_inject_fault_infinite('fts_update_config', 'error', 1);
-- FTS probe connection should encounter an error due to this fault,
-- injected on content 0 primary.
select gp_inject_fault_infinite('fts_handle_message', 'error', dbid)
from gp_segment_configuration where content = 0 and role = 'p';
-- Upon failure to probe content 0 primary, FTS will try to update the
-- configuration.  The update to configuration will hit error due to
-- the "fts_update_config" fault.
select gp_wait_until_triggered_fault('fts_update_config', 1, 1);

select gp_inject_fault('fts_handle_message', 'reset', dbid)
from gp_segment_configuration where content = 0 and role = 'p';

select gp_inject_fault('fts_update_config', 'reset', 1);

-- Postmaster should have restarted FTS by now. Trigger a scan and
-- validate that configuration is sane.
select gp_request_fts_probe_scan();

-- Verify that FTS didn't leak any locks due to the error during
-- config update.
select locktype, mode, relation, pid, granted from pg_locks where
relation = 'gp_segment_configuration'::regclass or
relation = 'gp_configuration_history'::regclass;

select count(*) = 2 as in_sync from gp_segment_configuration
where content = 0 and mode = 's';

alter system reset gp_fts_probe_interval;
alter system reset gp_fts_probe_retries;
select pg_reload_conf();
