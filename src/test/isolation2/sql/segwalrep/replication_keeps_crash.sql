-- Tests mark mirror down if replication walsender walreceiver keep
-- crash continuously.
--
-- Primary and mirror both alive, but wal replication crash happens
-- before start streaming data. And walsender, walreceiver keeps
-- re-connect continuously, this may block other processes.
-- Mark the mirror down to resolve it.

include: helpers/server_helpers.sql;

SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;

-- Error out before walsender streaming data
select gp_inject_fault_infinite('wal_sender_loop', 'error', dbid)
       from gp_segment_configuration where content=0 and role='p';

-- Should block in commit (SyncrepWaitForLSN()), waiting for commit
-- LSN to be flushed on mirror.
1&: create table mirror_block_t1 (a int) distributed by (a);

-- trigger failover
select gp_request_fts_probe_scan();

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
