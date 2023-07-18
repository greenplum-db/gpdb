-- Validate that the mirror replay process forwards fsync requests for
-- append-optimized tables to the checkpointer.  This is done using a
-- fault point that is reached only in a checkpointer process.  Note
-- that checkpointer process runs on primary as well as mirror.  We
-- are concerned with the checkpointer running on mirror in this test.
--
-- Starting with a clean slate (no pending fsync requests), the test
-- performs operations such as insert, vacuum, drop on AO row and
-- column oriented tables.  It validates that fsync was performed by
-- checkpointer for expected number of AO files at the end of each
-- operation.

-- Set the GUC to perform replay of checkpoint records immediately.
-- It speeds up the test.
!\retcode gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
-- Set fsync on since we need to test the fsync code logic.
!\retcode gpconfig -c fsync -v on --skipvalidation;
!\retcode gpstop -u;

create table fsync_ao(a int, b int) with (appendoptimized = true) distributed by (a);
create table fsync_co(a int, b int) with (appendoptimized = true, orientation = column) distributed by (a);
-- no fsync requests should ever be registered for unlogged tables
create unlogged table ul_fsync_co(a int, b int, c int) using ao_column distributed by (a);

insert into fsync_ao select i, i from generate_series(1,10)i;
insert into fsync_co select i, i from generate_series(1,10)i;

-- Fault to check that mirror has flushed pending fsync requests.
select gp_inject_fault_infinite('restartpoint_guts', 'skip', dbid)
	from gp_segment_configuration where role = 'm' and content = 0;

-- Start with a clean slate.
checkpoint;

-- Wait until restartpoint flush happens.
select gp_wait_until_triggered_fault('restartpoint_guts', 1, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- We have just created a checkpoint.  The next automatic checkpoint
-- will be triggered only after 5 minutes or after CheckPointSegments
-- wal segments.  Neither of that can happen until this test calls
-- explicit checkpoint.

-- Write ao and co data files including aoseg & gp_fastsequence.
-- These should be fsync-ed by checkpoint & restartpoint.
insert into fsync_ao select i, i from generate_series(1,20)i;
insert into fsync_co select i, i from generate_series(1,20)i;
insert into ul_fsync_co select i, i, i from generate_series(1,20)i;

-- Inject fault to count relfiles fsync'ed by checkpointer on mirror.
select gp_inject_fault_infinite('ao_fsync_counter', 'skip', dbid)
	from gp_segment_configuration where content=0 and role='m';

checkpoint;

-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 2, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Validate that the number of files fsync'ed by checkpointer (on
-- mirror).  This should match the number of files for fsync_ao and fsync_co.
select gp_wait_until_triggered_fault('ao_fsync_counter', 3, dbid)
	from gp_segment_configuration where content=0 and role='m';
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Test vacuum compaction with more than one segment file per table.
-- Perform concurrent inserts before vacuum to get multiple segment
-- files.  Validation criterion is the next checkpoint succeeds on the
-- mirror.
1: begin;
1: insert into fsync_ao select i, i from generate_series(1,20)i;
1: insert into fsync_co select i, i from generate_series(1,20)i;
1: insert into ul_fsync_co select i, i, i from generate_series(1,20)i;
insert into fsync_ao select i, i from generate_series(21,40)i;
insert into fsync_co select i, i from generate_series(21,40)i;
insert into ul_fsync_co select i, i, i from generate_series(1,40)i;
1: end;
-- Generate some invisible tuples in both the tables so as to trigger
-- compaction during vacuum.
delete from fsync_ao where a > 20;
update fsync_co set b = -a;
delete from ul_fsync_co where b < 20;

-- Expect two segment files for each table (ao table) or each column (co table).
select segment_id, segno, state from gp_toolkit.__gp_aoseg('fsync_ao');
select segment_id, segno, column_num, physical_segno, state from gp_toolkit.__gp_aocsseg('fsync_co');
select segment_id, segno, column_num, physical_segno, state from gp_toolkit.__gp_aocsseg('ul_fsync_co');
vacuum fsync_ao;
vacuum fsync_co;
vacuum ul_fsync_co;
checkpoint;
-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 3, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Expect the segment files that were updated by vacuum to be fsync'ed.
select gp_wait_until_triggered_fault('ao_fsync_counter', 12, dbid)
	from gp_segment_configuration where content=0 and role='m';
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Test that replay of drop table operation removes fsync requests
-- previously registed with the checkpointer.
update fsync_co set b = -a;
drop table fsync_co;
update ul_fsync_co set c = -a;
drop table ul_fsync_co;
-- Drop but don't commit the transaction.
begin;
update fsync_ao set b = -a;
drop table fsync_ao;
abort;
-- Fsync request for the following insert should not be forgotten.
insert into fsync_ao select * from generate_series(41,60)i;

checkpoint;
-- Wait until restartpoint happens again.
select gp_wait_until_triggered_fault('restartpoint_guts', 4, dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Expect that fsync is only performed for fsync_ao table (1 file) but
-- not for fsync_co table because it was dropped after being updated.
select gp_wait_until_triggered_fault('ao_fsync_counter', 13, dbid)
	from gp_segment_configuration where content=0 and role='m';
select gp_inject_fault('ao_fsync_counter', 'status', dbid)
	from gp_segment_configuration where content=0 and role='m';

-- Reset all faults.
select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration where content = 0;

!\retcode gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation;
!\retcode gpconfig -c fsync -v off --skipvalidation;
!\retcode gpstop -u;
