-- Tests for query conflict detection and cancellation on the hot standby.

----------------------------------------------------------------
-- Various query conflcit cases for hot standy.
--
-- All cases are written in this pattern:
-- 1. Start a standby transactin that will be conflicted and cancelled;
-- 2. Start a primary transaction that will conflict it;
-- 3. Commit the primary transaction. Since we are using remote_apply, it will 
--     wait until the WAL is applied on the standby, which would happen only
--     after the standby query is cancelled;
-- 4. Run something on the standby transaction and see the conflict error, which
--     in some cases it's ERROR, in others it's FATAL. 
-- 5. Re-run the cancelled query on the standby, and it can proceed now.
-- 6. Check the system view gp_stat_database_conflicts to see that the conflict
--     has been recorded. Note that we print the max count among all segments
--     to avoid flakiness.
-- See https://www.postgresql.org/docs/12/hot-standby.html#HOT-STANDBY-CONFLICT for more details.
----------------------------------------------------------------

-- We assume we start the test with clean records
-1S: select max(confl_tablespace), max(confl_lock), max(confl_snapshot), max(confl_bufferpin), max(confl_deadlock) from gp_stat_database_conflicts where datname = 'isolation2test';

-- explicit lock
create table hs_qc_lock(a int);
insert into hs_qc_lock select * from generate_series(1,5);
-1S: begin;
-1S: select * from hs_qc_lock;
1: begin;
1: lock table hs_qc_lock in access exclusive mode;
1: end;
-1S: select * from hs_qc_lock;
-1Sq:
-1S: select * from hs_qc_lock;
-1S: select max(confl_lock) from gp_stat_database_conflicts where datname = 'isolation2test';

-- lock through DDL
-1S: begin;
-1S: select * from hs_qc_lock;
1: alter table hs_qc_lock set access method ao_row;
-1S: select * from hs_qc_lock;
-1Sq:
-1S: select * from hs_qc_lock;
-1S: select max(confl_lock) from gp_stat_database_conflicts where datname = 'isolation2test';

-- drop database
1: create database hs_qc_dropdb;
-1Sq:
-1S:@db_name hs_qc_dropdb: select 1;
1: drop database hs_qc_dropdb;
-1S: select 1;
-1Sq:
-- Stats aren't counted for database conflicts. See: pgstat_recv_recoveryconflict

-- VACUUM of rows that the standby might be still seeing
1: create table hs_qc_vac1(a int);
1: insert into hs_qc_vac1 select * from generate_series(1,10);
-1S: begin transaction isolation level repeatable read;
-1S: select count(*) from hs_qc_vac1;
1: delete from hs_qc_vac1;
1: vacuum hs_qc_vac1;
-1S: select count(*) from hs_qc_vac1;
-1Sq:
-1S: select max(confl_snapshot) from gp_stat_database_conflicts where datname = 'isolation2test';

-- VACUUM of page that the standby is still holding buffer pin on, the difference with
-- the previous case is that here the deleted row is already invisible to the standby.
1: create table hs_qc_vac2(a int) distributed replicated;
1: insert into hs_qc_vac2 values(1),(2);
1: delete from hs_qc_vac2 where a = 1;
-- run select once on the standby, so the next select will fetch data from buffer
-1S: select count(*) from hs_qc_vac2;
-- suspend the standby at where it just unlocks the buffer but still holds the pin
1: select gp_inject_fault('heapgetpage_after_unlock_buffer', 'suspend',dbid) from gp_segment_configuration where content=0 and role='m';
-- we'll also make sure the startup process has sent out the signal before we let the standby backend release the pin
1: select gp_inject_fault('recovery_conflict_bufferpin_signal_sent', 'skip',dbid) from gp_segment_configuration where content=0 and role='m';
-1S&: select count(*) from hs_qc_vac2;
1: vacuum hs_qc_vac2;
-- as mentioned before, make sure startup process has sent the signal, and then let the standby proceed
1: select gp_wait_until_triggered_fault('recovery_conflict_bufferpin_signal_sent', 1,dbid) from gp_segment_configuration where content=0 and role='m';
1: select gp_inject_fault('recovery_conflict_bufferpin_signal_sent', 'reset',dbid) from gp_segment_configuration where content=0 and role='m';
1: select gp_inject_fault('heapgetpage_after_unlock_buffer', 'reset',dbid) from gp_segment_configuration where content=0 and role='m';
-- should see the conflict
-1S<:
-1Sq:
-1S: select max(confl_bufferpin) from gp_stat_database_conflicts where datname = 'isolation2test';

----------------------------------------------------------------
-- Test GUC hot_standby_feedback
----------------------------------------------------------------
!\retcode gpconfig -c hot_standby_feedback -v on;
!\retcode gpstop -u;

1: create table hs_qc_guc1(a int);
1: insert into hs_qc_guc1 select * from generate_series(1,10);

-1S: begin transaction isolation level repeatable read;
-1S: select * from hs_qc_guc1;

-- VACUUM won't cleanup this table since the standby still sees it
1: delete from hs_qc_guc1;
1: vacuum hs_qc_guc1;

-- hot standby can still see those rows
-1S: select * from hs_qc_guc1;

-- after the conflicting read transaction ends, the next VACUUM will successfully vacuum the table
-1S: end;
1: vacuum hs_qc_guc1;
-1S: select * from hs_qc_guc1;
-1Sq:

!\retcode gpconfig -r hot_standby_feedback;
!\retcode gpstop -u;

----------------------------------------------------------------
-- Test GUC vacuum_defer_cleanup_age
----------------------------------------------------------------
-- Use a GUC value that's not 0, so VACUUM does not clean up
-- recent dead rows that the hot standby might be still seeing.
!\retcode gpconfig -c vacuum_defer_cleanup_age -v 1;
!\retcode gpstop -u;

1: create table hs_qc_guc2(a int);
1: insert into hs_qc_guc2 select * from generate_series(1,10);

-1S: begin transaction isolation level repeatable read;
-1S: select count(*) from hs_qc_guc2;

-- VACUUM won't cleanup this table since the DELETE is still within vacuum_defer_cleanup_age
1: delete from hs_qc_guc2;
1: vacuum hs_qc_guc2;

-- hot standby can still query the table
-1S: select count(*) from hs_qc_guc2;

-- only if the age is reached, hot standby will see the same conflict as before
1: create temp table tt1(a int);
1: vacuum hs_qc_guc2;
-1S: select count(*) from hs_qc_guc2;
-1Sq:
-1S: select max(confl_snapshot) from gp_stat_database_conflicts where datname = 'isolation2test';

!\retcode gpconfig -r vacuum_defer_cleanup_age;
!\retcode gpstop -u;
