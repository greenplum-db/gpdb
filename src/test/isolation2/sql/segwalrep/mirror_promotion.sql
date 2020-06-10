-- Tests mirror promotion triggered by FTS in 2 different scenarios.
--
-- 1st: Shut-down of primary and hence unavailability of primary
-- leading to mirror promotion. In this case the connection between
-- primary and mirror is disconnected prior to promotion and
-- walreceiver doesn't exist.
--
-- 2nd: Primary is alive but using fault injector simulated to not
-- respond to fts. This helps to validate fts time-out logic for
-- probes. Plus also mirror promotion triggered while connection
-- between primary and mirror is still alive and hence walreceiver
-- also exist during promotion.

include: helpers/server_helpers.sql;

SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
-- stop a primary in order to trigger a mirror promotion
select pg_ctl((select datadir from gp_segment_configuration c
where c.role='p' and c.content=0), 'stop');

-- trigger failover
select gp_request_fts_probe_scan();

-- expect: to see the content 0, preferred primary is mirror and it's down
-- the preferred mirror is primary and it's up and not-in-sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- wait for content 0 (earlier mirror, now primary) to finish the promotion
0U: select 1;
-- Quit this utility mode session, as need to start fresh one below
0Uq:

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- expect: to see roles flipped and in sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- set GUCs to speed-up the test
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;
!\retcode gpconfig -c gp_fts_probe_timeout -v 5 --masteronly;
!\retcode gpstop -u;

-- start_ignore
select dbid from gp_segment_configuration where content = 0 and role = 'p';
-- end_ignore

select gp_inject_fault_infinite('fts_handle_message', 'infinite_loop', dbid)
from gp_segment_configuration
where content = 0 and role = 'p';

-- trigger failover
select gp_request_fts_probe_scan();

-- trigger one more probe right away which mostly results in sending
-- promotion request again to mirror, while its going through
-- promotion, which is nice condition to test as well.
select gp_request_fts_probe_scan();

-- expect segments restored back to its preferred role, but mirror is down
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- set GUCs to speed-up the test
!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpconfig -r gp_fts_probe_timeout --masteronly;
!\retcode gpstop -u;

-- -- wait for content 0 (earlier mirror, now primary) to finish the promotion
0U: select 1;

-- create tablespace to test if it works with gprecoverseg -F (pg_basebackup)
!\retcode mkdir -p /tmp/mirror_promotion_tablespace_loc;
create tablespace mirror_promotion_tablespace location '/tmp/mirror_promotion_tablespace_loc';
create table mirror_promotion_tblspc_heap_table (a int) tablespace mirror_promotion_tablespace;

-- -- now, let's fully recover the mirror
!\retcode gprecoverseg -aF  --no-progress;

drop table mirror_promotion_tblspc_heap_table;
drop tablespace mirror_promotion_tablespace;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- now, the content 0 primary and mirror should be at their preferred role
-- and up and in-sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;
