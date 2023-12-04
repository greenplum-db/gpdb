-- to make test deterministic and fast
!\retcode gpconfig -c gp_fts_mark_mirror_down_grace_period -v 0;
!\retcode gpstop -u;

-- Get an entry into gp_conf_history for a segment
-- start_ignore
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='m' and c.content=0), 'stop');
select gp_request_fts_probe_scan();
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='p' and c.content=0), 'stop');
select gp_request_fts_probe_scan();
select pg_ctl_start(datadir, port, false) from gp_segment_configuration where role = 'p' and content = 0;
select gp_request_fts_probe_scan();
-- end_ignore

!\retcode gprecoverseg -aF --no-progress;
!\retcode gprecoverseg -ar;

-- no segment down.
select count(*) from gp_segment_configuration where status = 'd';

select gp_request_fts_probe_scan();

-- note the last_timestamp in gp_configuration_history, we only need to check entries after this one
-1U: create table last_timestamp as select time from gp_configuration_history order by time desc limit 1;

-- stop primary in order to promote mirror for content 0
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='p' and c.content=0), 'stop');

select gp_request_fts_probe_scan();

-- primary is down, and mirror has now been promoted to primary. Verify
-1U: select wait_until_segments_are_down(1);
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- stop acting primary in order to trigger double fault for content 0
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='p' and c.content=0), 'stop');

-- trigger double fault on content 0 (FTS_PROBE_FAILED)
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- stop mirror for content 1
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='m' and c.content=1), 'stop');

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-1U: select wait_until_segments_are_down(2);

-- stop primary in order to trigger double fault for content 1
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='p' and c.content=1), 'stop');

-- trigger double fault on content 1 (FTS_PROMOTE_FAILED)
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

select pg_ctl_start(datadir, port, false) from gp_segment_configuration where role = 'p' and content = 0;

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

select pg_ctl_start(datadir, port, false) from gp_segment_configuration where role = 'p' and content = 1;

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF --no-progress;

!\retcode gprecoverseg -ar;

-- Test for when ftsprobe process is killed

-1U: drop table last_timestamp;

select gp_request_fts_probe_scan();

-- note the last_timestamp in gp_configuration_history, we only need to check entries after this one
-1U: create table last_timestamp as select time from gp_configuration_history order by time desc limit 1;

-- stop primary in order to promote mirror for content 0
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='p' and c.content=0), 'stop');

select gp_request_fts_probe_scan();

-- primary is down, and mirror has now been promoted to primary. Verify
-1U: select wait_until_segments_are_down(1);
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- stop acting primary in order to trigger double fault for content 0
select pg_ctl((select datadir from gp_segment_configuration c
               where c.role='p' and c.content=0), 'stop');

-- trigger double fault on content 0 (FTS_PROBE_FAILED)
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- kill the ftsprobe process.
!\retcode pkill -f ftsprobe;

-- restarts ftsprobe, we should see another entry for content 0 doublefault into gp_configuration_history
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

select pg_ctl_start(datadir, port, false) from gp_segment_configuration where role = 'p' and content = 0;
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF --no-progress;

!\retcode gprecoverseg -ar;

-1U: drop table last_timestamp;

!\retcode gpconfig -r gp_fts_mark_mirror_down_grace_period;
!\retcode gpstop -u;