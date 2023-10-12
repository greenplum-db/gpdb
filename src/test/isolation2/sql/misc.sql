--
-- Validate GPDB can create unique index on a table created in utility mode
--
-- NOTICE: we must connect to master in utility mode because the oid of table is
-- preassigned in QD, if we create a table in utility mode in QE, the oid might
-- conflict with preassigned oid.
-1U: create table utilitymode_primary_key_tab (c1 int);
-1U: create unique index idx_utilitymode_c1 on utilitymode_primary_key_tab (c1);
-1U: drop table utilitymode_primary_key_tab;

0U: explain analyze select * from gp_segment_configuration order by dbid;

--
-- Temp tables should have a different schema name pattern in utility mode.
--
-- A temp table's schema name used to be pg_temp_<session_id> in normal mode
-- and pg_temp_<backend_id> in utility mode, once the normal-mode session id
-- equals to the utility-mode backend id they will conflict with each other and
-- cause catalog corruption on the segment.
--
-- We have changed the name to pg_temp_0<backend_id> in utility mode.
0U: CREATE TEMP TABLE utilitymode_tmp_tab (c1 int) DISTRIBUTED BY (c1);
0U: SELECT substring(n.nspname FROM 1 FOR 9)
      FROM pg_namespace n
      JOIN pg_class c
        ON n.oid = c.relnamespace
     WHERE c.relname = 'utilitymode_tmp_tab';
0U: SELECT substring(n2.nspname FROM 1 FOR 15)
      FROM pg_namespace n1
      JOIN pg_class c
        ON n1.oid = c.relnamespace
      JOIN pg_namespace n2
        ON n2.nspname = 'pg_toast_temp_0' || substring(n1.nspname FROM 10)
     WHERE c.relname = 'utilitymode_tmp_tab';

--
-- Validate GPDB doesn't crash when creating partitioned table in utility mode
--
0U: create table utilitymode_pt_lt_tab (col1 int, col2 decimal)
	distributed by (col1) partition by list(col2) (partition part1 values(1));

--
-- gp_check_orphaned_files should not be running with concurrent transaction (even idle)
--
-- use a different database to do the test, otherwise we might be reporting tons 
-- of orphaned files produced by the many intential PANICs/restarts in the isolation2 tests.
create database check_orphaned_db;
1:@db_name check_orphaned_db: create extension gp_check_functions;
1:@db_name check_orphaned_db: begin;
2:@db_name check_orphaned_db: select * from gp_check_orphaned_files;
1q:
2q:

drop database check_orphaned_db;
