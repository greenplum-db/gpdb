-- Tests mark mirror down if replication walsender walreceiver keep
-- crash continuously.
--
-- Primary and mirror both alive, but wal replication crash happens
-- before start streaming data. And walsender, walreceiver keeps
-- re-connect continuously, this may block other processes.
-- GPDB will track the continuously failures for the replication.
-- If the failure times exceed a limitation, FTS will mark the mirror down
-- to avoid blocking other queries.
-- More details please refer to FTSGetReplicationDisconnectTime.

include: helpers/server_helpers.sql;

-- modify fts gucs to speed up the test.
1: alter system set gp_fts_probe_interval to 10;
1: alter system set gp_fts_replication_attempt_count to 3;
1: select pg_reload_conf();

SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;

-- Error out before walsender streaming data
select gp_inject_fault_infinite('wal_sender_loop', 'error', dbid)
       from gp_segment_configuration where content=0 and role='p';

-- Should block in commit (SyncrepWaitForLSN()), waiting for commit
-- LSN to be flushed on mirror.
1&: create table mirror_block_t1 (a int) distributed by (a);

select gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid) from gp_segment_configuration where role='p' and content=0;

-- trigger fts to mark mirror down.
select gp_request_fts_probe_scan();

-- After gp_fts_replication_attempt_count attempts mirror will be marked down, and syncrep will
-- be marked off and hence the commit should get unblocked.
-- Without gp_fts_replication_attempt_count mirror will continuously connect and re-connect and
-- be in grace period to not be marked down.
1<:

-- expect: to see the content 0, mirror is mark down
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

select gp_inject_fault('wal_sender_loop', 'reset', dbid)
       from gp_segment_configuration where content=0 and role='p';

-- -- now, let's recover the mirror
!\retcode gprecoverseg -a  --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;

drop table mirror_block_t1;

1: alter system reset gp_fts_probe_interval;
1: alter system reset gp_fts_replication_attempt_count;
1: select pg_reload_conf();
