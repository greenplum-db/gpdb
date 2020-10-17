-- expect: create table succeeds
create unlogged table unlogged_appendonly_table_managers (
	id int,
	name text
) with (
	appendonly=true
) distributed by (id);

-- skip FTS probes to make the test deterministic.
SELECT gp_inject_fault_infinite('fts_probe', 'skip', 1);
SELECT gp_request_fts_probe_scan();
SELECT gp_request_fts_probe_scan();
SELECT gp_wait_until_triggered_fault('fts_probe', 1, 1);

-- expect: insert/update/select works
insert into unlogged_appendonly_table_managers values (1, 'Joe');
insert into unlogged_appendonly_table_managers values (2, 'Jane');
update unlogged_appendonly_table_managers set name = 'Susan' where id = 2;
select * from unlogged_appendonly_table_managers order by id;
select segment_id, segno, tupcount from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');


-- force an unclean stop and recovery:
-- start_ignore
select restart_primary_segments_containing_data_for('unlogged_appendonly_table_managers');
-- end_ignore

-- expect inserts/updates are truncated after crash recovery
2: select * from unlogged_appendonly_table_managers;
2: select segment_id, segno, tupcount from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');


-- expect: insert/update/select works
3: insert into unlogged_appendonly_table_managers values (1, 'Joe');
3: insert into unlogged_appendonly_table_managers values (2, 'Jane');
3: update unlogged_appendonly_table_managers set name = 'Susan' where id = 2;
3: select * from unlogged_appendonly_table_managers order by id;
3: select segment_id, segno, tupcount from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');

-- force a clean stop and recovery:
-- start_ignore
select clean_restart_primary_segments_containing_data_for('unlogged_appendonly_table_managers');
-- end_ignore

-- expect: inserts/updates are persisted
4: select * from unlogged_appendonly_table_managers order by id;
4: select segment_id, segno, tupcount from gp_toolkit.__gp_aoseg('unlogged_appendonly_table_managers');

-- expect: drop table succeeds
5: drop table unlogged_appendonly_table_managers;

SELECT gp_inject_fault('fts_probe', 'reset', 1);
