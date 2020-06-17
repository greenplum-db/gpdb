-- start_matchsubs
--
-- # create a match/subs expression
--
-- m/ERROR:.*server closed the connection unexpectedly/
-- s/ERROR:.*server closed the connection unexpectedly/ERROR: server closed the connection unexpectedly/gm
-- end_matchsubs
include: helpers/server_helpers.sql;

3:SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
--
-- Test to validate crash at different points in AO/CO vacuum.
--
-- Setup tables to test crash at different points
-- for crash_before_cleanup_phase
3:set gp_default_storage_options="appendonly=true,orientation=column";
3:show gp_default_storage_options;
3:DROP TABLE IF EXISTS crash_before_cleanup_phase CASCADE;
3:CREATE TABLE crash_before_cleanup_phase (a INT, b INT, c CHAR(20));
3:CREATE INDEX crash_before_cleanup_phase_index ON crash_before_cleanup_phase(b);
3:INSERT INTO crash_before_cleanup_phase SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
3:DELETE FROM crash_before_cleanup_phase WHERE a < 4;
-- for crash_vacuum_in_appendonly_insert
3:DROP TABLE IF EXISTS crash_vacuum_in_appendonly_insert CASCADE;
3:CREATE TABLE crash_vacuum_in_appendonly_insert (a INT, b INT, c CHAR(20));
3:CREATE INDEX crash_vacuum_in_appendonly_insert_index ON crash_vacuum_in_appendonly_insert(b);
3:INSERT INTO crash_vacuum_in_appendonly_insert SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
3:UPDATE crash_vacuum_in_appendonly_insert SET b = 2;

-- inject panic fault.
3:SELECT gp_inject_fault('appendonly_insert', 'panic', '', '', 'crash_vacuum_in_appendonly_insert', 1, -1, 0, 2);

-- VACUUM on crash_before_cleanup_phase will end up skipping the drop
-- phase after not being able to acquire AccessExclusiveLock because
-- we make session 3 hold AccessShareLock. This results in segment
-- file 1 remaining in drop pending state which results in segment
-- file 1 not being scheduled for any new inserts.
3:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'suspend', '', '', 'crash_before_cleanup_phase', 1, -1, 0, 2);
3:BEGIN;
3:SELECT count(*) FROM crash_before_cleanup_phase;
1&:VACUUM crash_before_cleanup_phase;
3:SELECT gp_wait_until_triggered_fault('compaction_before_cleanup_phase', 1, 2);
3:END;

-- we already waited for suspend faults to trigger and hence we can proceed to
-- run next command which would trigger panic fault and help test
-- crash_recovery
3:VACUUM crash_vacuum_in_appendonly_insert;
1<:

-- wait for segment to complete recovering
0U: SELECT 1;
0Uq:

-- reset faults as protection incase tests failed and panic didn't happen
1:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'reset', 2);
1:SELECT gp_inject_fault('appendonly_insert', 'reset', 2);

-- perform post crash validation checks
-- for crash_before_cleanup_phase
1:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_before_cleanup_phase');
1:INSERT INTO crash_before_cleanup_phase VALUES(1, 1, 'c'), (25, 6, 'c');
1:UPDATE crash_before_cleanup_phase SET b = b+10 WHERE a=25;
1:SELECT * FROM crash_before_cleanup_phase ORDER BY a,b;
1:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_before_cleanup_phase');
-- This VACUUM removes the previous drop pending state for segment file 1 which
-- will make it available for future inserts.
1:VACUUM crash_before_cleanup_phase;
1:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_before_cleanup_phase');
1:INSERT INTO crash_before_cleanup_phase VALUES(21, 1, 'c'), (26, 1, 'c');
1:UPDATE crash_before_cleanup_phase SET b = b+10 WHERE a=26;
1:SELECT * FROM crash_before_cleanup_phase ORDER BY a,b;
-- crash_vacuum_in_appendonly_insert
1:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_vacuum_in_appendonly_insert');
1:VACUUM crash_vacuum_in_appendonly_insert;
1:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_vacuum_in_appendonly_insert');
1:INSERT INTO crash_vacuum_in_appendonly_insert VALUES(21, 1, 'c'), (26, 1, 'c');
1:UPDATE crash_vacuum_in_appendonly_insert SET b = b+10 WHERE a=26;
1:SELECT * FROM crash_vacuum_in_appendonly_insert ORDER BY a,b;

--
-- Setup tables to test crash at different points on master now
--
-- for crash_master_before_cleanup_phase
2:set gp_default_storage_options="appendonly=true,orientation=column";
2:show gp_default_storage_options;
2:DROP TABLE IF EXISTS crash_master_before_cleanup_phase CASCADE;
2:CREATE TABLE crash_master_before_cleanup_phase (a INT, b INT, c CHAR(20));
2:CREATE INDEX crash_master_before_cleanup_phase_index ON crash_master_before_cleanup_phase(b);
2:INSERT INTO crash_master_before_cleanup_phase SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
2:DELETE FROM crash_master_before_cleanup_phase WHERE a < 4;

-- inject panic fault 
2:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'panic', '', '', 'crash_master_before_cleanup_phase', 1, -1, 0, 1);
2:VACUUM crash_master_before_cleanup_phase;

-- reset faults as protection incase tests failed and panic didn't happen
4:SELECT gp_inject_fault('compaction_before_cleanup_phase', 'reset', 1);

-- perform post crash validation checks
-- for crash_master_before_cleanup_phase
4:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_master_before_cleanup_phase');
4:INSERT INTO crash_master_before_cleanup_phase VALUES(1, 1, 'c'), (25, 6, 'c');
4:UPDATE crash_master_before_cleanup_phase SET b = b+10 WHERE a=25;
4:SELECT * FROM crash_master_before_cleanup_phase ORDER BY a,b;
4:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_master_before_cleanup_phase');
4:VACUUM crash_master_before_cleanup_phase;
4:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_master_before_cleanup_phase');
4:INSERT INTO crash_master_before_cleanup_phase VALUES(21, 1, 'c'), (26, 1, 'c');
4:UPDATE crash_master_before_cleanup_phase SET b = b+10 WHERE a=26;
4:SELECT * FROM crash_master_before_cleanup_phase ORDER BY a,b;

-- Scenario for validating mirror replays fine and doesn't crash on
-- truncate record replay even if file is missing.
-- skip FTS probes to avoid marking primary status down.
4:SELECT gp_inject_fault_infinite('fts_probe', 'skip', 1);
4:SELECT gp_request_fts_probe_scan();
4:SELECT gp_wait_until_triggered_fault('fts_probe', 1, 1);
4:SET gp_default_storage_options="appendonly=true,orientation=column";
4:CREATE TABLE crash_vacuum_in_appendonly_insert_1 (a INT, b INT, c CHAR(20));
-- just sanity check to make sure appendonly table is created
4:SELECT count(*) from pg_appendonly where relid in (select oid from pg_class where relname='crash_vacuum_in_appendonly_insert_1');
4:INSERT INTO crash_vacuum_in_appendonly_insert_1 SELECT i AS a, 1 AS b, 'hello world' AS c FROM generate_series(1, 10) AS i;
4:UPDATE crash_vacuum_in_appendonly_insert_1 SET b = 2;
4:SELECT gp_inject_fault('xlog_ao_insert', 'infinite_loop', 2);
-- This will cause file to be created on primary for segno 2 but crash
-- just before creating the xlog record. Hence, primary will have the
-- file but not mirror.
4&:VACUUM crash_vacuum_in_appendonly_insert_1;
5:SELECT gp_wait_until_triggered_fault('xlog_ao_insert', 1, 2);
-- to make sure xlog gets flushed till this point to persist the
-- changes to pg_aocsseg.
5:CHECKPOINT;
-- Restart the primary to interrupt vacuum at that exact point.
5:select pg_ctl((select datadir from gp_segment_configuration c
where c.role='p' and c.content=0), 'restart');
4<:
-- Shows entries for new files added to pg_aocsseg table. These are
-- the entries next vacuum command will use to perform truncate.
0U:SELECT segno,column_num,physical_segno,tupcount,modcount,state FROM gp_toolkit.__gp_aocsseg('crash_vacuum_in_appendonly_insert_1');
-- generates truncate xlog record for all the files having entry in
-- pg_aocsseg table.
6:VACUUM crash_vacuum_in_appendonly_insert_1;
-- Make sure mirror is able to successfully replay all the truncate
-- records generated and doesn't encounter the "WAL contains
-- references to invalid pages" PANIC.
6:SELECT * from wait_for_replication_replay(0, 5000);
6:SELECT gp_inject_fault('fts_probe', 'reset', 1);
