----------------------------------------------------------------
-- Test transaction isolation in general, not specific to dtx
----------------------------------------------------------------
1: create table hs_tx(a int);
1: insert into hs_tx select * from generate_series(1,10);

1: begin;
1: insert into hs_tx select * from generate_series(11,20);
2: begin;
2: insert into hs_tx select * from generate_series(21,30);
2: abort;

-- standby should only see completed transactions, not in-progress transactions, nor aborted transactions
-1S: select * from hs_tx;

1: end;
-1S: select * from hs_tx;

----------------------------------------------------------------
-- Test isolation between hot standby query and in-progress dtx
----------------------------------------------------------------

1: create table hs_dtx1(a int);
1: create table hs_dtx2(a int);

-- inject two suspend faults:
-- 1. on seg0, suspend before PREPARE phase of 2PC
1: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',1,1,-1, dbid) from gp_segment_configuration where content=0 and role='p';
1&: insert into hs_dtx1 select * from  generate_series(1,10);
-- 2. on seg1, suspend before COMMIT phase of 2PC
2: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',2,2,-1, dbid) from gp_segment_configuration where content=1 and role='p';
2&: insert into hs_dtx2 select * from  generate_series(1,10);

-- standby should not see any rows from either dtx
-1S: select * from hs_dtx1;
-1S: select * from hs_dtx2;

-- reset
3: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=0 and role='p';
3: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=1 and role='p';
1<:
2<:

-- standby should see the results from the dtx now
-1S: select * from hs_dtx1;
-1S: select * from hs_dtx2;

----------------------------------------------------------------
-- Test DTX abort that happens in different phases
----------------------------------------------------------------

1: create table hs_abort_dtx1(a int);
1: create table hs_abort_dtx2(a int);

-- inject two errors:
-- 1. on seg0, error out before PREPARE phase of 2PC
1: select gp_inject_fault('exec_dtx_protocol_start', 'error','','','',1,1,-1, dbid) from gp_segment_configuration where content=0 and role='p';
1: insert into hs_abort_dtx1 select * from  generate_series(1,10);
1: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=0 and role='p';
-- 2. on seg1, error out before COMMIT phase of 2PC
1: select gp_inject_fault('exec_dtx_protocol_start', 'error','','','',2,2,-1, dbid) from gp_segment_configuration where content=1 and role='p';
1: insert into hs_abort_dtx2 select * from  generate_series(1,10);
1: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=1 and role='p';

-- standby should not see dtx1 which is aborted but should see dtx2 which is recovered
-1S: select * from hs_abort_dtx1;
-1S: select * from hs_abort_dtx2;

----------------------------------------------------------------
-- Test isolation between hot standby query and in-progress dtx,
-- but also run more queries in between
----------------------------------------------------------------
1: create table hs_dtx3(a int);

-- inject faults to suspend segments in 2PC
1: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',1,1,-1, dbid) from gp_segment_configuration where content=0 and role='p';
1&: insert into hs_dtx3 select * from  generate_series(1,10);
2: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',2,2,-1, dbid) from gp_segment_configuration where content=1 and role='p';
2&: insert into hs_dtx3 select * from  generate_series(11,20);

-- standby should not see rows in the in-progress dtx
-1S: select * from hs_dtx3;

-- now run some dtx and completed
3: insert into hs_dtx3 values(99);
3: create table hs_dtx4(a int);
3: insert into hs_dtx4 select * from  generate_series(1,10);

-- standby should still not see rows in the in-progress DTX, but should see the completed ones
-1S: select * from hs_dtx3;
-1S: select * from hs_dtx4;

3: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=0 and role='p';
3: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=1 and role='p';
1<:
2<:

-- standby should see all rows now
-1S: select * from hs_dtx3;

----------------------------------------------------------------
-- Test isolation between standby QD and in-progress dtx,
-- but after standby QD resets and gets running DTX from checkpoint.
-- Same tests will be repeated twice:
-- (1) with primary QD conducting a checkpoint;
-- (2) with standby QD conducting a checkpoint (restartpoint);
----------------------------------------------------------------
1: create table hs_t5(a int, b text);
1: create table hs_t6(a int, b text);

-- inject fault to suspend a primary right before it conducts the commit phase of 2PC,
-- so in the subsequent INSERT, all local transactions will be committed but the dtx is not.
1: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',2,2,-1, dbid) from gp_segment_configuration where content=0 and role='p';
1&: insert into hs_t5 select i, 'in-progress' from generate_series(1,10) i;

-- now run some dtx and completed, and primary conducts a checkpoint
2: insert into hs_t5 values(1, 'commited');
2: insert into hs_t6 select i, 'committed' from generate_series(1,10) i;
2: begin;
2: insert into hs_t5 values(99, 'aborted');
2: abort;
2: checkpoint;

-- now make the standby QD resets itself
-1S: select gp_inject_fault('exec_simple_query_start', 'panic', dbid) from gp_segment_configuration where content=-1 and role='m';
-1S: select 1;
-1Sq:

-- standby should still not see rows in the in-progress DTX, but should see the completed ones
-1S: select * from hs_t5;
-1S: select * from hs_t6;

2: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=0 and role='p';
1<:

-- standby should see all rows now
-1S: select * from hs_t5;
-1S: select * from hs_t6;

truncate hs_t5;
truncate hs_t6;

-- now repeat the above, but with standby QD conducting a checkpoint (restartpoint)
1: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',2,2,-1, dbid) from gp_segment_configuration where content=0 and role='p';
1&: insert into hs_t5 select i, 'in-progress' from generate_series(1,10) i;

2: insert into hs_t5 values(1, 'commited');
2: insert into hs_t6 select i, 'committed' from generate_series(1,10) i;
2: begin;
2: insert into hs_t5 values(99, 'aborted');
2: abort;

-1S: checkpoint;
-1S: select gp_inject_fault('exec_simple_query_start', 'panic', dbid) from gp_segment_configuration where content=-1 and role='m';
-1S: select 1;
-1Sq:

-- same result: standby should still not see rows in the in-progress DTX, but should see the completed ones
-1S: select * from hs_t5;
-1S: select * from hs_t6;

2: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=0 and role='p';
1<:

-- standby should see all rows now
-1S: select * from hs_t5;
-1S: select * from hs_t6;

-- more isolation test with in-progress dtx.
-- the purpose: Previously this would be fail because the standby gets a bumped nextGxid from checkpoint,
-- and moves its latestCompletedGxid too far (so that it thinks the new dtx already completed).
1: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',1,1,-1, dbid) from gp_segment_configuration where content=0 and role='p';
1&: delete from hs_t5;
2: select gp_inject_fault('exec_dtx_protocol_start', 'suspend','','','',2,2,-1, dbid) from gp_segment_configuration where content=1 and role='p';
2&: delete from hs_t6;
-- standby should not see the effect of the deletes
-1S: select * from hs_t5;
-1S: select * from hs_t6;
3: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=0 and role='p';
3: select gp_inject_fault('exec_dtx_protocol_start', 'reset',dbid) from gp_segment_configuration where content=1 and role='p';
1<:
2<:

----------------------------------------------------------------
-- Read-committed isolation: query on hot standby should not see dtx that completed after it
-- created distributed snapshot, but should see dtx that completed before that.
----------------------------------------------------------------

1: create table hs_rc(a int);
1: insert into hs_rc select * from generate_series(1,10);

-- case 1: suspend SELECT on the standby QD right after it created snapshot
-1S: select gp_inject_fault('select_after_qd_create_snapshot', 'suspend', dbid) from gp_segment_configuration where content=-1 and role='m';
-1S&: select * from hs_rc;

-- new INSERT or DELETE won't be observed by the standby
1: insert into hs_rc select * from generate_series(11,20);
1: delete from hs_rc where a < 5;
1: select gp_inject_fault('select_after_qd_create_snapshot', 'reset', dbid) from gp_segment_configuration where content=-1 and role='m';

-- should only see the rows at the time when SELECT started (1...10).
-1S<:

-- SELECT again, should see the effect from the INSERT and DELETE now
-1S: select * from hs_rc;

-- case 2: suspend SELECT on the standby QD before creating snapshot
-1S: select gp_inject_fault('exec_simple_query_start', 'suspend', dbid) from gp_segment_configuration where content=-1 and role='m';
-1S&: select * from hs_rc;

1: insert into hs_rc select * from generate_series(21,30);
1: delete from hs_rc where a < 21;
1: select gp_inject_fault('exec_simple_query_start', 'reset', dbid) from gp_segment_configuration where content=-1 and role='m';

-- standby should see the effect of the INSERT and DELETE
-1S<:

----------------------------------------------------------------
-- Read-committed isolation in the BEGIN...END block
----------------------------------------------------------------

1: truncate hs_rc;
1: insert into hs_rc select * from generate_series(1,30);

-1S: begin;
-1S: select count(*) from hs_rc;

-- have some concurrent sessions on primary QD:
-- 1. a completed transaction
1: delete from hs_rc where a <= 10;
-- 3. an aborted transaction
2: begin;
2: delete from hs_rc where a > 10 and a <= 20;
2: abort;
-- 3. an ongoing transaction
3: begin;
3: delete from hs_rc where a > 20 and a <= 30;

-- the standby should see results accordingly
-1S: select * from hs_rc;
-1S: end;

3: end;
-1S: select * from hs_rc;

----------------------------------------------------------------
-- Repeatable-read isolation: distributed snapshot is created at time of the 
-- first query in transaction block. All queries in the transaction block 
-- should only see results committed before the distributed snapshot creation.
----------------------------------------------------------------

1: create table hs_rr(a int);
1: insert into hs_rr select * from generate_series(1,10);

-1S: begin isolation level repeatable read;
-- should see 10
-1S: select count(*) from hs_rr;

-- do some more INSERT, DELETE and UPDATE
1: insert into hs_rr select * from generate_series(11,20);
1: delete from hs_rr where a <= 10;
1: update hs_rr set a = a + 100;

-- should still the initial rows {1...10}
-1S: select * from hs_rr;
-1S: end;

-- should see the results from the INSERT, DELETE and UPDATE
-1S: begin isolation level repeatable read;
-1S: select * from hs_rr;

-- standby won't see ongoing or aborted transactions either
1: begin;
1: insert into hs_rr select * from generate_series(1,10);
2: begin;
2: insert into hs_rr select * from generate_series(1,10);
2: abort;

-1S: select * from hs_rr;

1: end;
-1S: end;

----------------------------------------------------------------
-- Transaction isolation is respected in subtransactions too
----------------------------------------------------------------

1: create table hs_subtrx(a int);

-- (1) read-committed
-1S: begin;
-1S: select count(*) from hs_subtrx;
-1S: savepoint s1;

1: insert into hs_subtrx select * from generate_series(1,10);

-1S: select count(*) from hs_subtrx;
-1S: savepoint s2;
-1S: select count(*) from hs_subtrx;
-1S: rollback to savepoint s1;
-1S: select count(*) from hs_subtrx;
-1S: end;

-- (2) repeatable-read
-1S: begin isolation level repeatable read;
-1S: select * from hs_subtrx;
-1S: savepoint s1;

1: insert into hs_subtrx select * from generate_series(11,20);
1: delete from hs_subtrx where a <= 10;
1: update hs_subtrx set a = a + 100;

-1S: select * from hs_subtrx;
-1S: savepoint s2;
-1S: select * from hs_subtrx;
-1S: rollback to savepoint s1;
-1S: select * from hs_subtrx;
-1S: end;
-1S: select * from hs_subtrx;

----------------------------------------------------------------
-- Various isolation tests that involve AO/CO table.
----------------------------------------------------------------
1: create table hs_ao(a int, id int unique) using ao_row;
1: insert into hs_ao select 1,i from generate_series(1,10) i;
1: begin;
1: insert into hs_ao select 2,i from generate_series(11,20) i;

-- standby sees the same AO metadata as primary
2: select * from gp_toolkit.__gp_aoseg('hs_ao');
-1S: select * from gp_toolkit.__gp_aoseg('hs_ao');
2: select (gp_toolkit.__gp_aoblkdir('hs_ao')).* from gp_dist_random('gp_id');
-1S: select (gp_toolkit.__gp_aoblkdir('hs_ao')).* from gp_dist_random('gp_id');

-- standby sees correct table data
-1S: select * from hs_ao; 

-- standby sees the effect of vacuum
1: end;
1: delete from hs_ao where a = 1;
1: vacuum hs_ao;
1: select * from gp_toolkit.__gp_aoseg('hs_ao');
-1S: select * from gp_toolkit.__gp_aoseg('hs_ao');
-1S: select * from hs_ao; 
