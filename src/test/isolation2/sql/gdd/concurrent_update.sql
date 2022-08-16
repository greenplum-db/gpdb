CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Test concurrent update a table with a varying length type
CREATE TABLE t_concurrent_update(a int, b int, c char(84));
INSERT INTO t_concurrent_update VALUES(1,1,'test');

1: BEGIN;
1: SET optimizer=off;
1: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
2: SET optimizer=off;
2&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
1: END;
2<:
1: SELECT * FROM t_concurrent_update;
1q:
2q:

DROP TABLE t_concurrent_update;

-- Test the concurrent update transaction order on the segment is reflected on master
-- enable gdd
1: SHOW gp_enable_global_deadlock_detector;
1: CREATE TABLE t_concurrent_update(a int, b int);
1: INSERT INTO t_concurrent_update VALUES(1,1);

2: BEGIN;
2: SET optimizer=off;
2: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
3: BEGIN;
3: SET optimizer=off;
-- transaction 3 will wait transaction 2 on the segment
3&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;

-- transaction 2 suspend before commit, but it will wake up transaction 3 on segment
2: select gp_inject_fault('before_xact_end_procarray', 'suspend', '', 'isolation2test', '', 1, 1, 0, dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
2&: END;
1: select gp_wait_until_triggered_fault('before_xact_end_procarray', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
-- transaction 3 should wait transaction 2 commit on master
3<:
3&: END;
-- the query should not get the incorrect distributed snapshot: transaction 1 in-progress
-- and transaction 2 finished
1: SELECT * FROM t_concurrent_update;
1: select gp_inject_fault('before_xact_end_procarray', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content=-1;
2<:
3<:
2q:
3q:

1: SELECT * FROM t_concurrent_update;
1q:

-- Same test as the above, except the first transaction commits before the
-- second transaction check the wait gxid, it should get the gxid from
-- pg_distributedlog instead of the procarray.
4: BEGIN;
4: SET optimizer=off;
4: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;

5: BEGIN;
5: SET optimizer=off;
-- suspend before get 'wait gxid'
5: SELECT gp_inject_fault('before_get_distributed_xid', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content=1;
5&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;

6: SELECT gp_wait_until_triggered_fault('before_get_distributed_xid', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content=1;
4: END;
4: SELECT gp_inject_fault('before_get_distributed_xid', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content=1;

5<:
5: END;
6: SELECT * FROM t_concurrent_update;
6: DROP TABLE t_concurrent_update;
4q:
5q:
6q:

-- Test update distkey
-- IF we enable the GDD, then the lock maybe downgrade to
-- RowExclusiveLock, when we UPDATE the distribution keys,
-- A SplitUpdate node will add to the Plan, then an UPDATE
-- operator may split to DELETE and INSERT.
-- IF we UPDATE the distribution keys concurrently, the
-- DELETE operator will not execute EvalPlanQual and the
-- INSERT operator can not be *blocked*, so it will
-- generate more tuples in the tables.
-- We raise an error when the GDD is enabled and the
-- distribution keys is updated.

0: create table tab_update_hashcol (c1 int, c2 int) distributed by(c1);
0: insert into tab_update_hashcol values(1,1);
0: select * from tab_update_hashcol;

1: begin;
2: begin;
1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
1: end;
2<:
2: end;
0: select * from tab_update_hashcol;
0: drop table tab_update_hashcol;

-- Test EvalplanQual
-- If we enable the GDD, then the lock maybe downgrade to
-- RowExclusiveLock, so UPDATE/Delete can be executed
-- concurrently, it may trigger the EvalPlanQual function
-- to recheck the qualifications.
-- If the subPlan have Motion node, then we can not execute
-- EvalPlanQual correctly, so we raise an error when
-- GDD is enabled and EvalPlanQual is tiggered.

0: create table tab_update_epq1 (c1 int, c2 int) distributed randomly;
0: create table tab_update_epq2 (c1 int, c2 int) distributed randomly;
0: insert into tab_update_epq1 values(1,1);
0: insert into tab_update_epq2 values(1,1);
0: select * from tab_update_epq1;
0: select * from tab_update_epq2;

1: set optimizer = off;
2: set optimizer = off;

1: begin;
2: begin;
1: update tab_update_epq1 set c1 = c1 + 1 where c2 = 1;
2&: update tab_update_epq1 set c1 = tab_update_epq1.c1 + 1 from tab_update_epq2 where tab_update_epq1.c2 = tab_update_epq2.c2;
1: end;
2<:
2: end;

0: select * from tab_update_epq1;
0: drop table tab_update_epq1;
0: drop table tab_update_epq2;
0q:
1q:
2q:

-- Currently the DML node (modification variant in ORCA) doesn't support EPQ
-- routine so concurrent modification is not possible in this case. User have
-- to be informed to fallback to postgres optimizer.
-- This test makes sense just for ORCA
create table test as select 0 as i distributed randomly;
-- in session 1, turn off the optimizer so it will invoke heap_update
1: set optimizer = off;
1: begin;
1: update test set i = i + 1;
-- in session 2, in case of ORCA DML invokes EPQ
-- the following SQL will hang due to XID lock
2&: delete from test where i = 0;
-- commit session1's transaction so the above session2 will continue and emit
-- error about serialization failure in case of ORCA
1: end;
2<:
drop table test;
1q:
2q:

-- split update is to implement updating on hash keys,
-- it deletes the tuple and insert a new tuple in a
-- new segment, so it is not easy for other transaction
-- to follow the update link to fetch the new tuple. The
-- other transaction should raise error for such case.
-- the following case should be tested with GDD enabled.
-- See github issue: https://github.com/greenplum-db/gpdb/issues/8919

0:create table t_splitupdate_raise_error (a int, b int) distributed by (a);
0:insert into t_splitupdate_raise_error values (1, 1);

-- test delete will throw error
1: begin;
1: update t_splitupdate_raise_error set a = a + 1;

2: begin;
2&: delete from t_splitupdate_raise_error;

1: end;
2<:

2: abort;
1q:
2q:

-- test norm update will throw error
1: begin;
1: update t_splitupdate_raise_error set a = a + 1;

2: begin;
2&: update t_splitupdate_raise_error set b = 999;

1: end;
2<:

2: abort;
1q:
2q:

0:drop table t_splitupdate_raise_error;

-- test eval planqual with initPlan
-- EvalPlanQualStart will init all subplans even if it does
-- not use it. If such subplan contains Motion nodes, we should
-- error out just like the behavior in EvalPlanQual.
-- See Issue: https://github.com/greenplum-db/gpdb/issues/12902
-- for details.
1: create table t_epq_subplans(a int, b int);
1: insert into t_epq_subplans values (1, 1);
1: begin;
1: update t_epq_subplans set b = b + 1;

-- make sure planner will contain InitPlan
-- NOTE: orca does not generate InitPlan.
2: explain update t_epq_subplans set b = b + 1 where a > -1.5 * (select max(a) from t_epq_subplans);
2&: update t_epq_subplans set b = b + 1 where a > -1.5 * (select max(a) from t_epq_subplans);

1: end;
-- session 2 should throw error and not PANIC
2<:

1: drop table t_epq_subplans;

1q:
2q:
