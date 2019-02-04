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

-- create heap table
0: show gp_enable_global_deadlock_detector;
0: create table tab_update_hashcol (c1 int, c2 int) distributed by(c1);
0: insert into tab_update_hashcol values(1,1);
0: select * from tab_update_hashcol;

-- test for heap table
1: begin;
2: begin;
1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
1: end;
2<:
2: end;

0: select * from tab_update_hashcol;
0: drop table tab_update_hashcol;

-- create AO table
0: create table tab_update_hashcol (c1 int, c2 int) with(appendonly=true) distributed by(c1);
0: insert into tab_update_hashcol values(1,1);
0: select * from tab_update_hashcol;

-- test for AO table
1: begin;
2: begin;
1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
1: end;
2<:
2: end;
0: select * from tab_update_hashcol;
0: drop table tab_update_hashcol;
1q:
2q:
0q:

-- enable gdd
-- start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v on;
! gpstop -rai;
-- end_ignore

-- create heap table
0: show gp_enable_global_deadlock_detector;
0: create table tab_update_hashcol (c1 int, c2 int) distributed by(c1);
0: insert into tab_update_hashcol values(1,1);
0: select * from tab_update_hashcol;

-- test for heap table
1: begin;
2: begin;
1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
1: end;
2<:
2: end;
0: select * from tab_update_hashcol;
0: drop table tab_update_hashcol;

-- create AO table
0: create table tab_update_hashcol (c1 int, c2 int) with(appendonly=true) distributed by(c1);
0: insert into tab_update_hashcol values(1,1);
0: select * from tab_update_hashcol;

-- test for AO table
1: begin;
2: begin;
1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;
1: end;
2<:
2: end;
0: select * from tab_update_hashcol;
0: drop table tab_update_hashcol;
1q:
2q:
0q:

-- disable gdd
-- start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v off;
! gpstop -rai;
-- end_ignore
