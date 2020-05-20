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

include: helpers/server_helpers.sql;
-- Table to just store the master's data directory path on segment,
-- this is used to restart master to enable GDD.
CREATE TABLE ccud_datadir(a int, dir text);
INSERT INTO ccud_datadir select 1,datadir from gp_segment_configuration where role='p' and content=-1;

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
ALTER SYSTEM SET gp_enable_global_deadlock_detector TO ON;
1U:SELECT pg_ctl(dir, 'restart') from ccud_datadir;

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

-- disable gdd
0:ALTER SYSTEM RESET gp_enable_global_deadlock_detector;
0q:
1U:SELECT pg_ctl(dir, 'restart') from ccud_datadir;
0:show gp_enable_global_deadlock_detector;
