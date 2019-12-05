-- This test validates that AO tables are sync'ed by checkpoint.
-- It simulates the following scenario.
--
--   * Start with a clean slate - ensure that all files are flushed by checkpointer.
--   * Write two tables (one is ao and another is aoco).
--   * Verify that those files were also fsync-ed by restartpoint on mirror.
--   * Checkpoint followed by vacuum on tables with multiple segment files.

-- Set the GUC to perform replay of checkpoint records immediately.  It speeds up the test.
-- Set fsync on since we need to test the fsync code logic.
!\retcode gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
!\retcode gpconfig -c fsync -v on --skipvalidation;
!\retcode gpstop -u;

-- Reset all faults.
select gp_inject_fault('all', 'reset', dbid)
	from gp_segment_configuration where content = 0;

-- Fault to check that mirror has flushed pending fsync requests.
select gp_inject_fault_infinite('restartpoint_guts', 'skip', dbid)
	from gp_segment_configuration where role = 'm' and content = 0;

-- Start with a clean slate.
checkpoint;

-- We have just created a checkpoint.  The next checkpoint will be triggered
-- only after 5 minutes or after CheckPointSegments wal segments.  Neither of
-- that can happen until this test calls explicit checkpoint.

-- Wait until restartpoint flush happens.
select gp_wait_until_triggered_fault('restartpoint_guts', 1, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Validate that fsync is performed only on the primary when the GUC
-- to do so is not set.  Also validate that on the mirror, the
-- checkpointer process performs fsync for AO, regardless of the GUC.

-- Expect the GUC to be off by default.
show forward_ao_fsync_on_primary;

create table fsync_ao(a int, b int) with (appendoptimized = true) distributed by (a);
create table fsync_co(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
-- Temp table segment files should never be fsync'ed.
create temp table no_fsync_ao(a int, b int)
       with (appendoptimized = true) distributed by (a);
insert into fsync_ao select i, i from generate_series(1,20)i;
insert into fsync_co select i, i from generate_series(1,20)i;
insert into no_fsync_ao select i, i from generate_series(1,20)i;

-- Inject fault to count relfiles fsync'ed by checkpointer on primary
-- as well as on mirror.
select gp_inject_fault_infinite('ao_fsync_counter', 'skip', dbid)
	from gp_segment_configuration where content=0;

checkpoint;

-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 2, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Validate that the number of files fsync'ed by checkpointer on
-- primary.  `num times hit` is corresponding to the number of files
-- noticed by `ao_fsync_counter` fault.  It should be 0 because by
-- default, fsync of AO data files should be performed by the backend
-- processes writing to them on primary.
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0 and role='p';
-- Checkpointer on mirror should perfrom fsync for three AO segment
-- files.
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Enable fsync forward GUC so that fsync requests for
-- append-optimized tables are forwarded to checkpointer on primary.
!\retcode gpconfig -c forward_ao_fsync_on_primary -v on --skipvalidation;
!\retcode gpstop -au;

-- Write ao and co data files including temp ao table.  Only the
-- non-temp files should be fsync-ed by checkpoint & restartpoint.
insert into fsync_ao select i, i from generate_series(1,20)i;
insert into fsync_co select i, i from generate_series(1,20)i;
insert into no_fsync_ao select i, i from generate_series(1,20)i;

checkpoint;

-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 3, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Validate that the number of files fsync'ed by checkpointer.  `num
-- times hit` is corresponding to the number of files synced by
-- `ao_fsync_counter` fault.  It should be non-zero on both, primary
-- and mirror.
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0;

-- Test vacuum compaction with more than one segment file per table.  Perform
-- concurrent inserts before vacuum to get multiple segment files.  Validation
-- criterion is the checkpoint command succeeds on primary and the
-- restartpoint_guts fault point is reached on the mirror.
1: begin;
1: insert into fsync_ao select i, i from generate_series(1,20)i;
1: insert into fsync_co select i, i from generate_series(1,20)i;
insert into fsync_ao select i, i from generate_series(21,40)i;
insert into fsync_co select i, i from generate_series(21,40)i;
1: end;
-- Generate some invisible tuples in both the tables so as to trigger
-- compaction during vacuum.
delete from fsync_ao where a > 20;
update fsync_co set b = -a;
-- Expect two segment files for each table (ao table) or each column (co table).
select segno, state from gp_toolkit.__gp_aoseg('fsync_ao');
select segno, column_num, physical_segno, state from gp_toolkit.__gp_aocsseg('fsync_co');
vacuum fsync_ao;
vacuum fsync_co;
checkpoint;
-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 4, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Expect the segment files that were updated by vacuum to be fsync'ed.
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0;

-- Checkpoint after drop table should be successful. It validates that the drop
-- removed the fsync requests enqued by the previous insert from
-- pendingOpsTable.
insert into fsync_co select i, i from generate_series(1,20)i;
drop table fsync_co;
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
2:@db_name fsync_ao_db: insert into fsync_ao select i, i from generate_series(1,20)i;
2:@db_name fsync_ao_db: insert into fsync_co select i, i from generate_series(1,20)i;
2q:
drop database fsync_ao_db;
checkpoint;
-- Wait until restartpoint happens again.  Create database creates two
-- and drop database creates one checkpoint.  So wait until the
-- restartpoint happens three times on mirror.
select gp_wait_until_triggered_fault('restartpoint_guts', 7, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Reset all faults.
select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration where content = 0;

!\retcode gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation;
!\retcode gpconfig -r fsync --skipvalidation;
!\retcode gpconfig -r forward_ao_fsync_on_primary --skipvalidation;
!\retcode gpstop -u;
