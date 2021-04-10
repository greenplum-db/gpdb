-- This test performs segment reconfiguration when a distributed 
-- transaction is in progress. The expectation is that the first 
-- command in the transaction after reconfiguration should fail. It 
-- verifies a bug where a stale gang was reused in such a case, if the 
-- failed primary happened to be up and listening.

-- set these values purely to cut down test time, as default ts trigger is
-- every min and 5 retries
alter system set gp_fts_probe_interval to 10;
alter system set gp_fts_probe_retries to 0;
select pg_reload_conf();

create table test_fts_session_reset(c1 int);

1:BEGIN;
-- let the dispatcher create a gang
1:insert into test_fts_session_reset select * from generate_series(1,20);
-- this injected fault can make dispatcher think the primary is down
2:select gp_inject_fault('fts_conn_startup_packet', 'error', dbid) from gp_segment_configuration where role='p' and content=0;
2:select gp_request_fts_probe_scan();
2:select status = 'd' from gp_segment_configuration where content = 0 and role = 'm';
-- At this point, content 0 mirror is promoted and the primary is marked down. 
-- the gang used by the previous insert is no longer valid. It must be destroyed 
-- and the transaction must be aborted.
1:insert into test_fts_session_reset select * from generate_series(21,40);
1:select count(*) from test_fts_session_reset;
1:END;
1:select count(*) from test_fts_session_reset;
2:select gp_inject_fault('fts_conn_startup_packet', 'reset', dbid) from gp_segment_configuration where content=0;
1q:
2q:

-- expect one primary is down and mirror is promoted to primary
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0 order by role;

!\retcode gprecoverseg -aF --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- verify no segment is down after recovery
select count(*) from gp_segment_configuration where status = 'd';

alter system reset gp_fts_probe_interval;
alter system reset gp_fts_probe_retries;
select pg_reload_conf();

