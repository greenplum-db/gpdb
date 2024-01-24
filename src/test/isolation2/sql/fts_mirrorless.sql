-- Get an entry into gp_conf_history for any segment
select pg_ctl((select datadir from gp_segment_configuration c
               where c.content=0), 'stop');
select gp_request_fts_probe_scan();
select pg_ctl_start(datadir, port) from gp_segment_configuration where role = 'p' and content = 0;
select gp_request_fts_probe_scan();

-- Start of test. Bring two segments down, check entries in gp_configuration_history
-- no segment down.
select count(*) from gp_segment_configuration where status = 'd';

select gp_request_fts_probe_scan();

-- note the last_timestamp in gp_configuration_history, we only need to check entries after this one
-1U: create table last_timestamp as select time from gp_configuration_history order by time desc limit 1;

-- stop segment for content 0
select pg_ctl((select datadir from gp_segment_configuration c
               where c.content=0), 'stop');

select gp_request_fts_probe_scan();

-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- stop segment for content 1
select pg_ctl((select datadir from gp_segment_configuration c
               where c.content=1), 'stop');

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

select pg_ctl_start(datadir, port) from gp_segment_configuration where role = 'p' and content = 0;

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

select pg_ctl_start(datadir, port) from gp_segment_configuration where role = 'p' and content = 1;

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-1U: drop table last_timestamp;

select gp_request_fts_probe_scan();

-- note the last_timestamp in gp_configuration_history, we only need to check entries after this one
-1U: create table last_timestamp as select time from gp_configuration_history order by time desc limit 1;

-- stop primary for content 0
select pg_ctl((select datadir from gp_segment_configuration c
               where c.content=0), 'stop');

select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-- kill the ftsprobe process.
!\retcode pkill -f ftsprobe;

-- restarts ftsprobe, we should see another entry for content 0 doublefault into gp_configuration_history
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

select pg_ctl_start(datadir, port) from gp_segment_configuration where role = 'p' and content = 0;
select gp_request_fts_probe_scan();
-1U: select dbid, description from gp_configuration_history where time > (select time from last_timestamp) order by time;

-1U: drop table last_timestamp;
