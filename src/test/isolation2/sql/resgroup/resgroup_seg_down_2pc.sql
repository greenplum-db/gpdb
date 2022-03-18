-- This test performs segment reconfiguration when "alter resource group" is executed in the two phase commit.
-- The steps are, when run "alter resource group", before QD broadcasts commit prepared command to QEs(the 
-- second phase of 2PC), we trigger an error and cause one primary segment down. 
-- The expectation is "alter resource group" can run successfully since the mirror segment is UP. 
-- After recover the segment, there is no error or blocking.

create extension if not exists gp_inject_fault;

-- set these values purely to cut down test time, as default fts trigger is
-- every min and 5 retries
alter system set gp_fts_probe_interval to 10;
alter system set gp_fts_probe_retries to 0;
select pg_reload_conf();

1:create resource group rgroup_seg_down with (CPU_RATE_LIMIT=35, MEMORY_LIMIT=35, CONCURRENCY=10);

-- inject an error in function dtm_broadcast_commit_prepared, that is before QD broadcasts commit prepared command to QEs
2:select gp_inject_fault_infinite('dtm_broadcast_commit_prepared', 'suspend', dbid) from gp_segment_configuration where role='p' and content=-1;
-- this session will pend here since the above injected fault
1&:alter resource group rgroup_seg_down set CONCURRENCY 20;
-- this injected fault can make dispatcher think the primary is down
2:select gp_inject_fault('fts_conn_startup_packet', 'error', dbid) from gp_segment_configuration where role='p' and content=0;
2:select gp_request_fts_probe_scan();
-- make sure one primary segment is down.
2:select status = 'd' from gp_segment_configuration where content = 0 and role = 'm';
-- reset the injected fault on QD and the "alter resource group" in session1 can continue
2:select gp_inject_fault('dtm_broadcast_commit_prepared', 'reset', dbid) from gp_segment_configuration where role='p' and content=-1;
-- No need to reset fts_conn_startup_packet fault inject because the segment we have set
-- is down now, and later we will full recover it back with new init clean shared memory.
1<:
-- make sure "alter resource group" has taken effect.
1:select concurrency from gp_toolkit.gp_resgroup_config where groupname = 'rgroup_seg_down';
2q:

!\retcode gprecoverseg -aF --no-progress;

-- loop while segments come in sync
1:select wait_until_all_segments_synchronized();

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
1:select wait_until_all_segments_synchronized();

-- verify no segment is down after recovery
1:select count(*) from gp_segment_configuration where status = 'd';

-- verify resource group 
1:select concurrency from gp_toolkit.gp_resgroup_config where groupname = 'rgroup_seg_down';
1:drop resource group rgroup_seg_down;

1:alter system reset gp_fts_probe_interval;
1:alter system reset gp_fts_probe_retries;
1:select pg_reload_conf();
1q:

