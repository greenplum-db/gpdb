-- Distributed snapshot tests

-- Scenario1: Test to validate GetSnapshotData()'s computation of globalXmin using
-- distributed snapshot. It mainly uses a old read-only transaction to help
-- create situation where globalXmin can be lower than distributed oldestXmin
-- when calling DistributedLog_AdvanceOldestXmin().

-- Setup
CREATE TABLE distributed_snapshot_test1 (a int);

1: BEGIN;
1: INSERT INTO distributed_snapshot_test1 values(1);
-- Read transaction which helps to get lower globalXmin for session 3. As this
-- will have MyProc->xmin set to transaction 1's xid.
2: BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
2: SELECT * from distributed_snapshot_test1;
-- Complete transaction 1, so that it no more appears in in-progress transaction
-- list for following transactions.
1: COMMIT;

-- Transaction to bump the latestCompletedXid
1: INSERT INTO distributed_snapshot_test1 values(1);

-- Hold after walking over ProcArray in GetSnpashotData(), right at start of
-- DistributedLog_AdvanceOldestXmin()
1: SELECT gp_inject_fault('distributedlog_advance_oldest_xmin', 'suspend',
   '', 'postgres', '', 1, -1, 5, dbid) from gp_segment_configuration
   where content = 0 and role = 'p';
3&:@db_name postgres: SELECT count(*) > 0 from gp_dist_random('gp_id');
1: SELECT gp_wait_until_triggered_fault('distributedlog_advance_oldest_xmin', 1, dbid)
   from gp_segment_configuration where content = 0 and role = 'p';
2: COMMIT;

-- Transaction used to bump the distributed oldestXmin
1: INSERT INTO distributed_snapshot_test1 values(1);
-- let session 3 now move forward to compute distributed oldest xmin
1: SELECT gp_inject_fault('distributedlog_advance_oldest_xmin', 'reset', dbid)
   from gp_segment_configuration where content = 0 and role = 'p';
3<:

-- Scenario2: This scenario tests the boundary condition for Xmax in distributed snapshot

-- Setup
CREATE TABLE distributed_snapshot_test2 (a int);

-- start transaction assigns distributed xid.
1: BEGIN ISOLATION LEVEL REPEATABLE READ;
-- this sets latestCompletedXid
2: INSERT INTO distributed_snapshot_test2 VALUES(1);
-- here, take distributed snapshot
1: SELECT 123 AS "establish snapshot";
2: INSERT INTO distributed_snapshot_test2 VALUES(2);
-- expected to see just VALUES(1)
1: SELECT * FROM distributed_snapshot_test2;
1: COMMIT;

DROP TABLE distributed_snapshot_test2;

-- Scenario3: Test the one-phase commit transactions don't break repeatable read isolation.
--
-- Direct dispatch causes the select statements to be dispatched only to one of
-- the three demo cluster segments. A segment acquires local snapshot only when it
-- receives the dispatched statement. If one phase commit relied on local
-- snapshots only, wrong results are possible depending on the order of local
-- snapshot acquisition by the segments. This scenario validates that distributed
-- snapshot is used by the segments to evaluate tuple visibility in case of
-- one-phase commit and correct results are returned.
--
-- connection 40 inserts 100, 100 and 300 serially using one-phase commit
-- protocol. Repeatable read transactions may read (100), (100,100) or
-- (100,100,300), but not (100, 300).
CREATE TABLE distributed_snapshot_test3 (a int);
10: BEGIN ISOLATION LEVEL REPEATABLE READ;
20: BEGIN ISOLATION LEVEL REPEATABLE READ;
30: BEGIN ISOLATION LEVEL REPEATABLE READ;
40: INSERT INTO distributed_snapshot_test3 VALUES(100);
10: SELECT gp_segment_id, * FROM distributed_snapshot_test3 where a = 100;
40: INSERT INTO distributed_snapshot_test3 VALUES(100);
30: SELECT 123 AS "establish snapshot";
40: INSERT INTO distributed_snapshot_test3 VALUES(300);
10: SELECT gp_segment_id, * FROM distributed_snapshot_test3;
20: SELECT gp_segment_id, * FROM distributed_snapshot_test3;
30: SELECT gp_segment_id, * FROM distributed_snapshot_test3;
10: COMMIT;
20: COMMIT;
30: COMMIT;
DROP TABLE distributed_snapshot_test3;

-- The following test cases are to test that QEs can get
-- latest distribute snapshot to scan normal tables (not catalog).
-- Greenplum tests the visibility of heap tuples firstly using
-- distributed snapshot. Distributed snapshot is generated on
-- QD and then dispatched to QEs. Some utility statement needs
-- to work under latest snapshot when executing, so that they
-- invoke the function `GetLatestSnapshot` in QEs. But remember
-- we cannot get the latest distributed snapshot.

-- Subtle cases are: Alter Table or Alter Domain statements on QD
-- get snapshot in Portal Run and then try to hold locks on the
-- target table in ProcessUtilitySlow. Here is the key point:
--   1. try to hold lock ==> it might be blocked by other transcations
--   2. then it will be waked up to continue
--   3. when it can continue, the world has changed because other transcations
--      then blocks it have been over

-- Previously, on QD we do not getsnapshot before we dispatch utility
-- statement to QEs which leads to the distributed snapshot does not
-- reflect the "world change". This will lead to some bugs. For example,
-- if the first transaction is to rewrite the whole heap, and then
-- the second Alter Table or Alter Domain statements continues with
-- the distributed snapshot that txn1 does not commit yet, it will
-- see no tuples in the new heap!
-- See Github issue https://github.com/greenplum-db/gpdb/issues/10216

-- Now this has been fixed, the following cases are tests to check this.

-- Case 1: concurrently alter column type (will do rewrite heap) 
create table t_alter_snapshot_test(a int, b int, c int);
insert into t_alter_snapshot_test values (1, 1, 1), (1, 1, 1);

select * from t_alter_snapshot_test;

1: begin;
1: alter table t_alter_snapshot_test alter column b type text;

-- the following statement will hang
2&: alter table t_alter_snapshot_test alter column c type text;

1: end;
-- after 1 commit, 2 can continue, it should use latest distributed
-- snapshot so that the data will not be lost.
2<:

select * from t_alter_snapshot_test;
drop table t_alter_snapshot_test;

-- Case 2: concurrently add exclude constrain
create table t_alter_snapshot_test(a int, b int);
insert into t_alter_snapshot_test values (1, 1), (1, 1);

select a from t_alter_snapshot_test;

1: begin;
1: alter table t_alter_snapshot_test alter column b type int using b::int;

2&: alter table t_alter_snapshot_test add exclude using btree (a WITH =);

1: end;
-- after 1 commit, 2 can go on and it should fail
2<:

drop table t_alter_snapshot_test;

-- Case 3: concurrently split partition
create table t_alter_snapshot_test(id int, rank int, year int)
distributed by (id)
partition by range (year)
( start (0) end (20) every (4), default partition extra );

insert into t_alter_snapshot_test select i,i,i from generate_series(1, 100)i;
select count(*) from t_alter_snapshot_test;

1: begin;
1: alter table t_alter_snapshot_test alter column rank type text;

2&: alter table t_alter_snapshot_test split partition for (5) at (5)  into (partition pa,  partition pb);

1: end;
-- after 1 commit, 2 can go on and it should not lose data
2<:

select count(*) from t_alter_snapshot_test;
drop table t_alter_snapshot_test;

-- case 4: concurrently validate check
create table t_alter_snapshot_test(a int, b int);
insert into t_alter_snapshot_test values (1, 1), (2, 2);
alter table t_alter_snapshot_test ADD CONSTRAINT mychk CHECK(a > 20) NOT VALID;

1: begin;
1: alter table t_alter_snapshot_test alter column b type text;

2&: alter table t_alter_snapshot_test validate CONSTRAINT mychk;

1: end;
-- after 1 commit, 2 can go on and it should fail
2<:

drop table t_alter_snapshot_test;

-- case 5: concurrently domain check
create domain domain_snapshot_test as int;
create table t_alter_snapshot_test(i domain_snapshot_test, j int, k int);
insert into t_alter_snapshot_test values(200,1,1);
alter domain domain_snapshot_test ADD CONSTRAINT mychk CHECK(VALUE > 300)  NOT VALID;

1: begin;
1: alter table t_alter_snapshot_test alter column k type text;

2&: alter domain domain_snapshot_test validate CONSTRAINT mychk;

1:end;
-- after 1 commit, 2 can go on and it should fail
2<:

drop table t_alter_snapshot_test;
drop domain domain_snapshot_test;

-- case 6: alter table expand table
create table t_alter_snapshot_test(a int, b int);
set allow_system_table_mods = on;
update gp_distribution_policy set numsegments = 2 where localoid = 't_alter_snapshot_test'::regclass::oid;
insert into t_alter_snapshot_test select i,i from generate_series(1, 10)i;
select gp_segment_id, * from t_alter_snapshot_test;

1: begin;
1: alter table t_alter_snapshot_test alter column b type text;

2&: alter table t_alter_snapshot_test expand table;

1: end;
-- after 1 commit, 2 can go on and data should not be lost
2<:

select gp_segment_id, * from t_alter_snapshot_test;
drop table t_alter_snapshot_test;

-- case 7: alter table set distributed by
create table t_alter_snapshot_test(a int, b int) distributed randomly;
insert into t_alter_snapshot_test select i,i from generate_series(1, 10)i;
select count(*) from t_alter_snapshot_test;

1: begin;
1: alter table t_alter_snapshot_test alter column b type text;

2&: alter table t_alter_snapshot_test set distributed by (a);

1: end;
-- after 1 commit, 2 can continue and data should not be lost
2<:

select count(*) from t_alter_snapshot_test;
drop table t_alter_snapshot_test;

-- case 8: DML concurrent with Alter Table
create table t_alter_snapshot_test(a int, b int);

---- test for insert
1: begin;
1: insert into t_alter_snapshot_test values (1, 1);
2&: alter table t_alter_snapshot_test alter column b type text;
1: end;
-- 2 can continue, and we should not lose data
2<:
select * from t_alter_snapshot_test;

---- test for update
truncate t_alter_snapshot_test;
insert into t_alter_snapshot_test values (1, 1);
1: begin;
1: update t_alter_snapshot_test set b = '3';
2&: alter table t_alter_snapshot_test alter column b type int using b::int;
1: end;
-- 2 can continue and we should see the data has been updated
2<:
select * from t_alter_snapshot_test;

---- test for delete
truncate t_alter_snapshot_test;
insert into t_alter_snapshot_test values (1, 1);
1: begin;
1: delete from t_alter_snapshot_test;
2&: alter table t_alter_snapshot_test alter column b type text;
1: end;
-- 2 can continue and we should see the data has been deleted
2<:
select * from t_alter_snapshot_test;
drop table t_alter_snapshot_test;

-- Case 9: Repeatable Read Isolation Level Test
create table t_alter_snapshot_test(a int, b int);
insert into t_alter_snapshot_test values (1, 1);
1: begin;
1: insert into t_alter_snapshot_test values (1, 1);

2: begin isolation level repeatable read;
2: select * from  t_alter_snapshot_test;
2&: alter table t_alter_snapshot_test alter column b type text;

1: end;
-- 2 can continue and after its alter rewrite the heap
-- it can see all the data even under repeatable read
2<:
2: select * from t_alter_snapshot_test;
2: end;

select * from t_alter_snapshot_test;
drop table t_alter_snapshot_test;
