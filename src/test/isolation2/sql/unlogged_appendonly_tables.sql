-- helpers:
include: helpers/server_helpers.sql;

-- expect: create table succeeds
create unlogged table unlogged_appendonly_table_managers (
	id int,
	name text
) with (
	appendonly=true
) distributed by (id);

-- skip FTS probes to make the test deterministic.
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
SELECT gp_inject_fault_infinite('fts_probe', 'skip', 1);
SELECT gp_request_fts_probe_scan();
SELECT gp_request_fts_probe_scan();
SELECT gp_wait_until_triggered_fault('fts_probe', 1, 1);

-- expect: insert/update/select works
insert into unlogged_appendonly_table_managers values (1, 'Joe');
insert into unlogged_appendonly_table_managers values (2, 'Jane');
update unlogged_appendonly_table_managers set name = 'Susan' where id = 2;
select * from unlogged_appendonly_table_managers order by id;
select count(1) from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');


-- force an unclean stop and recovery:
-- start_ignore
select restart_primary_segments_containing_data_for('unlogged_appendonly_table_managers');
-- end_ignore

-- expect inserts/updates are truncated after crash recovery
2: select * from unlogged_appendonly_table_managers;
2: select count(1) from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');


-- expect: insert/update/select works
3: insert into unlogged_appendonly_table_managers values (1, 'Joe');
3: insert into unlogged_appendonly_table_managers values (2, 'Jane');
3: update unlogged_appendonly_table_managers set name = 'Susan' where id = 2;
3: select * from unlogged_appendonly_table_managers order by id;
3: select count(1) from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');

-- force a clean stop and recovery:
-- start_ignore
select clean_restart_primary_segments_containing_data_for('unlogged_appendonly_table_managers');
-- end_ignore

-- expect: inserts/updates are persisted
4: select * from unlogged_appendonly_table_managers order by id;
4: select count(1) from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');

SELECT gp_inject_fault('fts_probe', 'reset', 1);

-- test truncate for unlogged tables
5: truncate unlogged_appendonly_table_managers;

-- set GUCs to speed-up the test
!\retcode gpconfig -c gp_fts_probe_retries -v 2 --masteronly;
!\retcode gpconfig -c gp_fts_probe_timeout -v 5 --masteronly;
!\retcode gpstop -u;

-- failover to mirror
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
-- Quit this utility mode session
0Uq:

-- test: can insert and select from the table
5: select * from unlogged_appendonly_table_managers;
5: insert into unlogged_appendonly_table_managers select i, 'b' from generate_series(1, 10)i;
5: select * from unlogged_appendonly_table_managers;

-- fully recover the failed primary as new mirror
!\retcode gprecoverseg -aF --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

5: truncate unlogged_appendonly_table_managers;

-- now revert back to original configuration
!\retcode gprecoverseg -ar --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- test: can insert and select from the table
6: select * from unlogged_appendonly_table_managers;
6: insert into unlogged_appendonly_table_managers select i, 'b' from generate_series(1, 10)i;
6: select * from unlogged_appendonly_table_managers;

-- expect: drop table succeeds
6: drop table unlogged_appendonly_table_managers;

!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpconfig -r gp_fts_probe_timeout --masteronly;
!\retcode gpstop -u;
