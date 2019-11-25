-- This test validates that AO tables are sync'ed by checkpoint.
-- It simulates the following scenario.
--
--   * Quickly test temp table should work fine with the fsync mechanism on ao/co (ignored).
--   * Start with a clean slate - ensure that all files are flushed by checkpointer.
--   * Write two tables (one is ao and another is aoco).
--   * Resume checkpointer and let it fsync the two dirty AO relations and their auxiliary tables.
--   * Verify that 6 files (gp_fastsequence, ao data files, aoseg files) were fsync'ed by checkpointer.
--   * Verify that those files were also fsync-ed by restartpoint on mirror.
--   * Insert tuples, drop the tables; then run checkpoint (on all rest tests also).
--   * Verify that the fsync mechanism works for that (FORGET_RELATION_FSYNC).
--   * Vacuum on the tables with multiple segment files due to concurrent insert.
--   * Verify that the fsync mechanism works for that.
--   * Re-test by creating and dropping database.
--   * Verify that the fsync mechanism works for that (FORGET_DATABASE_FSYNC).

-- Set the GUC to perform replay of checkpoint records immediately.  It speeds up the test.
-- Set fsync on since we need to test the fsync code logic.
!\retcode gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
!\retcode gpconfig -c fsync -v on --skipvalidation;
!\retcode gpstop -u;

-- Quicly test that temp table should work fine with the fsync mechanism on ao/co.
create temp table t_fsync_ao(a int, b int) with (appendoptimized = true) distributed by (a);
create temp table t_fsync_co(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
insert into t_fsync_ao select i, i from generate_series(1,10)i;
insert into t_fsync_co select i, i from generate_series(1,10)i;
checkpoint;
drop table t_fsync_ao;
drop table t_fsync_co;
checkpoint;

-- Below are all tests for normal tables.

create table fsync_ao(a int, b int) with (appendoptimized = true) distributed by (a);
create table fsync_co(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
insert into fsync_ao select i, i from generate_series(1,10)i;
insert into fsync_co select i, i from generate_series(1,10)i;

-- Reset all faults.
-- Note: we use gp_inject_fault_infinite here instead of
-- gp_inject_fault so cache of pg_proc that contains
-- gp_inject_fault_infinite is loaded before checkpoint and
-- the following gp_inject_fault_infinite don't dirty the
-- buffer again.
select gp_inject_fault_infinite('all', 'reset', dbid)
	from gp_segment_configuration where content = 0;

-- Skip hint bits setting on primaries so that no fsync request due to hint
-- bits happen in the below tests.
select gp_inject_fault_infinite('set_hint_bits', 'skip', dbid)
	from gp_segment_configuration where role = 'p' and content = 0;

-- Fault to check that mirror has flushed pending fsync requests.
select gp_inject_fault_infinite('restartpoint_guts', 'skip', dbid)
	from gp_segment_configuration where role = 'm' and content = 0;

-- Start with a clean slate.
checkpoint;

-- Wait until restartpoint flush happens.
select gp_wait_until_triggered_fault('restartpoint_guts', 1, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- We have just created a checkpoint.  The next checkpoint will be triggered
-- only after 5 minutes or after CheckPointSegments wal segments.  Neither of
-- that can happen until this test calls explicit checkpoint.

-- Write ao and co data files including aoseg & gp_fastsequence.
-- These should be fsync-ed by checkpoint & restartpoint.
insert into fsync_ao select i, i from generate_series(1,10)i;
insert into fsync_co select i, i from generate_series(1,10)i;

-- Inject fault to count relfiles fsync'ed by checkpointer on primary as well
-- as mirror.
select gp_inject_fault_infinite('fsync_counter', 'skip', dbid)
	from gp_segment_configuration where content = 0;

checkpoint;

-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 2, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Validate that the number of files fsync'ed by checkpointer.
-- `num times hit` is corresponding to the number of files synced by
-- `fsync_counter` fault type.
select gp_inject_fault('fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0;

-- Checkpoint after drop table should be successful. It validates that the drop
-- removed the fsync requests enqued by the previous insert from
-- pendingOpsTable.
insert into fsync_co select i, i from generate_series(1,10)i;
drop table fsync_co;
checkpoint;
-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 3, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Test vacuum compaction with more than one segment file per table.  Perform
-- concurrent inserts before vacuum to get multiple segment files.  Validation
-- criterion is the checkpoint command succeeds on primary and the
-- restartpoint_guts fault point is reached on the mirror.
create table fsync_co(a int, b int) with (appendoptimized = true, orientation =
column) distributed by (a);
1: begin;
1: insert into fsync_ao select i, i from generate_series(1,10)i;
1: insert into fsync_co select i, i from generate_series(1,10)i;
insert into fsync_ao select i, i from generate_series(1,10)i;
insert into fsync_co select i, i from generate_series(1,10)i;
1: end;
-- expect two segment files for each table (ao table) or each column (co table).
select segno, state from gp_toolkit.__gp_aoseg('fsync_ao');
select segno, column_num, physical_segno, state from gp_toolkit.__gp_aocsseg('fsync_co');
vacuum fsync_ao;
vacuum fsync_co;
checkpoint;
-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 4, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Checkpoint after drop database should be successful. It validates that the drop
-- removed the fsync requests enqued by the previous insert from
-- pendingOpsTable.
create database fsync_ao_db;
2:@db_name fsync_ao_db: create table fsync_ao(a int, b int) with (appendoptimized = true) distributed by (a);
2:@db_name fsync_ao_db: create table fsync_co(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
2:@db_name fsync_ao_db: insert into fsync_ao select i, i from generate_series(1,10)i;
2:@db_name fsync_ao_db: insert into fsync_co select i, i from generate_series(1,10)i;
2q:
drop database fsync_ao_db;
checkpoint;
-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 5, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Reset all faults.
select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration where content = 0;

!\retcode gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation;
!\retcode gpconfig -r fsync --skipvalidation;
!\retcode gpstop -u;
