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
1: CREATE TABLE t_concurrent_update(a int, b int);
1: INSERT INTO t_concurrent_update VALUES(1,1);

2: BEGIN;
2: SET optimizer=off;
2: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
3: BEGIN;
3: SET optimizer=off;
-- transaction 3 will wait transaction 1 on the segment
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
1: SELECT count(*) FROM t_concurrent_update;
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
