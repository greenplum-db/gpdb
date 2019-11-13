-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/ERROR:.*server closed the connection unexpectedly (cdbdispatchresult.c:\d+)/
-- s/ERROR:.*server closed the connection unexpectedly (cdbdispatchresult.c:\d+)/ERROR: server closed the connection unexpectedly (cdbdispatchresult.c:XXX)/gm
-- end_matchsubs
3:CREATE extension if NOT EXISTS gp_inject_fault;
3:SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
--
-- Test to validate crash at different points in AO/CO vacuum.
--
-- Setup tables to test crash at different points
-- for crash_before_cleanup_phase
3:set gp_default_storage_options="appendonly=true,orientation=row";
3:show gp_default_storage_options;
3:DROP TABLE IF EXISTS crash_before_cleanup_phase CASCADE;
3:CREATE TABLE crash_before_cleanup_phase (a INT, b INT, c CHAR(20));
3:CREATE INDEX crash_before_cleanup_phase_index ON crash_before_cleanup_phase(b);
3:INSERT INTO crash_before_cleanup_phase SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
3:DELETE FROM crash_before_cleanup_phase WHERE a < 4;
-- for crash_before_segmentfile_drop
3:DROP TABLE IF	EXISTS crash_before_segmentfile_drop CASCADE;
3:CREATE TABLE crash_before_segmentfile_drop (a INT, b INT, c CHAR(20));
3:CREATE INDEX crash_before_segmentfile_drop_index ON crash_before_segmentfile_drop(b);
3:INSERT INTO crash_before_segmentfile_drop SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
3:DELETE FROM crash_before_segmentfile_drop WHERE a < 4;
-- for crash_vacuum_in_appendonly_insert
3:DROP TABLE IF EXISTS crash_vacuum_in_appendonly_insert CASCADE;
3:CREATE TABLE crash_vacuum_in_appendonly_insert (a INT, b INT, c CHAR(20));
3:CREATE INDEX crash_vacuum_in_appendonly_insert_index ON crash_vacuum_in_appendonly_insert(b);
3:INSERT INTO crash_vacuum_in_appendonly_insert SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
3:UPDATE crash_vacuum_in_appendonly_insert SET b = 2;

-- suspend at intended points. Only one AO table can be in drop phase in a
-- system at a time hence we must wait for this table's VACUUM to reach cleanup
-- phase before triggering another VACUUM.
3:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'suspend', '', '', 'crash_before_cleanup_phase', 1, -1, 0, 2);
1&:VACUUM crash_before_cleanup_phase;
3:SELECT gp_wait_until_triggered_fault('compaction_before_cleanup_phase', 1, 2);

3:SELECT gp_inject_fault('compaction_before_segmentfile_drop', 'suspend', '', '', 'crash_before_segmentfile_drop', 1, -1, 0, 2);
2&:VACUUM crash_before_segmentfile_drop;
3:SELECT gp_wait_until_triggered_fault('compaction_before_segmentfile_drop', 1, 2);

-- we already waited for suspend faults to trigger and hence we can proceed to
-- run next command which would trigger panic fault and help test
-- crash_recovery
3:SELECT gp_inject_fault('appendonly_insert', 'panic', '', '', 'crash_vacuum_in_appendonly_insert', 1, -1, 0, 2);
3:VACUUM crash_vacuum_in_appendonly_insert;
1<:
2<:

-- wait for segment to complete recovering
0U: SELECT 1;

-- reset faults as protection incase tests failed and panic didn't happen
1:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'reset', 2);
1:SELECT gp_inject_fault('compaction_before_segmentfile_drop', 'reset', 2);
1:SELECT gp_inject_fault('appendonly_insert', 'reset', 2);

-- perform post crash validation checks
-- for crash_before_cleanup_phase
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_before_cleanup_phase');
1:INSERT INTO crash_before_cleanup_phase VALUES(1, 1, 'c'), (25, 6, 'c');
1:UPDATE crash_before_cleanup_phase SET b = b+10 WHERE a=25;
1:SELECT * FROM crash_before_cleanup_phase ORDER BY a,b;
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_before_cleanup_phase');
1:VACUUM crash_before_cleanup_phase;
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_before_cleanup_phase');
1:INSERT INTO crash_before_cleanup_phase VALUES(21, 1, 'c'), (26, 1, 'c');
1:UPDATE crash_before_cleanup_phase SET b = b+10 WHERE a=26;
1:SELECT * FROM crash_before_cleanup_phase ORDER BY a,b;
-- for crash_before_segmentfile_drop
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_before_segmentfile_drop');
1:INSERT INTO crash_before_segmentfile_drop VALUES(1, 1, 'c'), (25, 6, 'c');
1:UPDATE crash_before_segmentfile_drop SET b = b+10 WHERE a=25;
1:SELECT * FROM crash_before_segmentfile_drop ORDER BY a,b;
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_before_segmentfile_drop');
1:VACUUM crash_before_segmentfile_drop;
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_before_segmentfile_drop');
1:INSERT INTO crash_before_segmentfile_drop VALUES(21, 1, 'c'), (26, 1, 'c');
1:UPDATE crash_before_segmentfile_drop SET b = b+10 WHERE a=26;
1:SELECT * FROM crash_before_segmentfile_drop ORDER BY a,b;
-- crash_vacuum_in_appendonly_insert
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_vacuum_in_appendonly_insert');
1:VACUUM crash_vacuum_in_appendonly_insert;
1:SELECT * FROM gp_toolkit.__gp_aoseg('crash_vacuum_in_appendonly_insert');
1:INSERT INTO crash_vacuum_in_appendonly_insert VALUES(21, 1, 'c'), (26, 1, 'c');
1:UPDATE crash_vacuum_in_appendonly_insert SET b = b+10 WHERE a=26;
1:SELECT * FROM crash_vacuum_in_appendonly_insert ORDER BY a,b;

--
-- Setup tables to test crash at different points on master now
--
-- for crash_master_before_cleanup_phase
2:set gp_default_storage_options="appendonly=true,orientation=row";
2:show gp_default_storage_options;
2:DROP TABLE IF EXISTS crash_master_before_cleanup_phase CASCADE;
2:CREATE TABLE crash_master_before_cleanup_phase (a INT, b INT, c CHAR(20));
2:CREATE INDEX crash_master_before_cleanup_phase_index ON crash_master_before_cleanup_phase(b);
2:INSERT INTO crash_master_before_cleanup_phase SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
2:DELETE FROM crash_master_before_cleanup_phase WHERE a < 4;
-- for crash_master_before_segmentfile_drop
2:DROP TABLE IF EXISTS crash_master_before_segmentfile_drop CASCADE;
2:CREATE TABLE crash_master_before_segmentfile_drop (a INT, b INT, c CHAR(20));
2:CREATE INDEX crash_master_before_segmentfile_drop_index ON crash_master_before_segmentfile_drop(b);
2:INSERT INTO crash_master_before_segmentfile_drop SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
2:DELETE FROM crash_master_before_segmentfile_drop WHERE a < 4;

-- suspend at intended points
2:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'suspend', '', '', 'crash_master_before_cleanup_phase', 1, -1, 0, 1);
1&:VACUUM crash_master_before_cleanup_phase;
2:SELECT gp_inject_fault('compaction_before_segmentfile_drop', 'panic', '', '', 'crash_master_before_segmentfile_drop', 1, -1, 0, 1);

-- wait for suspend faults to trigger and then proceed to run next
-- command which would trigger panic fault and help test
-- crash_recovery
SELECT gp_wait_until_triggered_fault('compaction_before_cleanup_phase', 1, 1);
2:VACUUM crash_master_before_segmentfile_drop;
1<:

-- reset faults as protection incase tests failed and panic didn't happen
4:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'reset', 1);
4:SELECT gp_inject_fault('compaction_before_segmentfile_drop', 'reset', 1);

-- perform post crash validation checks
-- for crash_master_before_cleanup_phase
4:SELECT * FROM gp_toolkit.__gp_aoseg('crash_master_before_cleanup_phase');
4:INSERT INTO crash_master_before_cleanup_phase VALUES(1, 1, 'c'), (25, 6, 'c');
4:UPDATE crash_master_before_cleanup_phase SET b = b+10 WHERE a=25;
4:SELECT * FROM crash_master_before_cleanup_phase ORDER BY a,b;
4:SELECT * FROM gp_toolkit.__gp_aoseg('crash_master_before_cleanup_phase');
4:VACUUM crash_master_before_cleanup_phase;
4:SELECT * FROM gp_toolkit.__gp_aoseg('crash_master_before_cleanup_phase');
4:INSERT INTO crash_master_before_cleanup_phase VALUES(21, 1, 'c'), (26, 1, 'c');
4:UPDATE crash_master_before_cleanup_phase SET b = b+10 WHERE a=26;
4:SELECT * FROM crash_master_before_cleanup_phase ORDER BY a,b;
-- for crash_master_before_segmentfile_drop
4:SELECT * FROM gp_toolkit.__gp_aoseg('crash_master_before_segmentfile_drop');
4:INSERT INTO crash_master_before_segmentfile_drop VALUES(1, 1, 'c'), (25, 6, 'c');
4:UPDATE crash_master_before_segmentfile_drop SET b = b+10 WHERE a=25;
4:SELECT * FROM crash_master_before_segmentfile_drop ORDER BY a,b;
4:SELECT * FROM gp_toolkit.__gp_aoseg('crash_master_before_segmentfile_drop');
4:VACUUM crash_master_before_segmentfile_drop;
4:SELECT * FROM gp_toolkit.__gp_aoseg('crash_master_before_segmentfile_drop');
4:INSERT INTO crash_master_before_segmentfile_drop VALUES(21, 1, 'c'), (26, 1, 'c');
4:UPDATE crash_master_before_segmentfile_drop SET b = b+10 WHERE a=26;
4:SELECT * FROM crash_master_before_segmentfile_drop ORDER BY a,b;
