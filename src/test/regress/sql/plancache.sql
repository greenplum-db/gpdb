--
-- Tests to exercise the plan caching/invalidation mechanism
--

CREATE TEMP TABLE pcachetest AS SELECT * FROM int8_tbl;

-- create and use a cached plan
PREPARE prepstmt AS SELECT * FROM pcachetest;

EXECUTE prepstmt;

-- and one with parameters
PREPARE prepstmt2(bigint) AS SELECT * FROM pcachetest WHERE q1 = $1;

EXECUTE prepstmt2(123);

-- invalidate the plans and see what happens
DROP TABLE pcachetest;

EXECUTE prepstmt;
EXECUTE prepstmt2(123);

-- recreate the temp table (this demonstrates that the raw plan is
-- purely textual and doesn't depend on OIDs, for instance)
CREATE TEMP TABLE pcachetest AS SELECT * FROM int8_tbl;

EXECUTE prepstmt;
EXECUTE prepstmt2(123);

-- prepared statements should prevent change in output tupdesc,
-- since clients probably aren't expecting that to change on the fly
ALTER TABLE pcachetest ADD COLUMN q3 bigint;

EXECUTE prepstmt;
EXECUTE prepstmt2(123);

-- but we're nice guys and will let you undo your mistake
ALTER TABLE pcachetest DROP COLUMN q3;

EXECUTE prepstmt;
EXECUTE prepstmt2(123);

-- Try it with a view, which isn't directly used in the resulting plan
-- but should trigger invalidation anyway
CREATE TEMP VIEW pcacheview AS
  SELECT * FROM pcachetest;

PREPARE vprep AS SELECT * FROM pcacheview;

EXECUTE vprep;

CREATE OR REPLACE TEMP VIEW pcacheview AS
  SELECT q1, q2/2 AS q2 FROM pcachetest;

EXECUTE vprep;

-- Check basic SPI plan invalidation

create function cache_test(int) returns int as $$
declare total int;
begin
	create temp table t1(f1 int) distributed by (f1);
	insert into t1 values($1);
	insert into t1 values(11);
	insert into t1 values(12);
	insert into t1 values(13);
	select sum(f1) into total from t1;
	drop table t1;
	return total;
end
$$ language plpgsql;

select cache_test(1);
select cache_test(2);
select cache_test(3);

-- Check invalidation of plpgsql "simple expression"

create temp view v1 as
  select 2+2 as f1;

create function cache_test_2() returns int as $$
begin
	return f1 from v1;
end$$ language plpgsql;

select cache_test_2();

create or replace temp view v1 as
  select 2+2+4 as f1;
select cache_test_2();

create or replace temp view v1 as
  select 2+2+4+(select max(unique1) from tenk1) as f1;
select cache_test_2();

--- Check that change of search_path is honored when re-using cached plan

create schema s1
  create table abc (f1 int);

create schema s2
  create table abc (f1 int);

insert into s1.abc values(123);
insert into s2.abc values(456);

set search_path = s1;

prepare p1 as select f1 from abc;

execute p1;

set search_path = s2;

select f1 from abc;

execute p1;

alter table s1.abc add column f2 float8;   -- force replan

execute p1;

drop schema s1 cascade;
drop schema s2 cascade;

reset search_path;

-- Check that invalidation deals with regclass constants

create temp sequence seq;

prepare p2 as select nextval('seq');

execute p2;

drop sequence seq;

create temp sequence seq;

execute p2;

-- Check DDL via SPI, immediately followed by SPI plan re-use
-- (bug in original coding)

create function cachebug() returns void as $$
declare r int;
begin
  drop table if exists temptable cascade;
  -- Ignore NOTICE about missing DISTRIBUTED BY. It was annoying here, as
  -- usually you would only see it on the first invocation, but sometimes
  -- you'd also get it on the second invocation, if the plan cache
  -- got invalidated in between the invocations.
  set client_min_messages=warning;
  create temp table temptable as select * from generate_series(1,3) as f1;
  reset client_min_messages;
  create temp view vv as select * from temptable;
  for r in select * from vv order by f1 loop
    raise notice '%', r;
  end loop;
end$$ language plpgsql;

select cachebug();
select cachebug();

-- Check that addition or removal of any partition is correctly dealt with by
-- default partition table when it is being used in prepared statement.
create table pc_list_parted (a int) partition by list(a);
create table pc_list_part_null partition of pc_list_parted for values in (null);
create table pc_list_part_1 partition of pc_list_parted for values in (1);
create table pc_list_part_def partition of pc_list_parted default;
prepare pstmt_def_insert (int) as insert into pc_list_part_def values($1);
-- should fail
execute pstmt_def_insert(null);
execute pstmt_def_insert(1);
create table pc_list_part_2 partition of pc_list_parted for values in (2);
execute pstmt_def_insert(2);
alter table pc_list_parted detach partition pc_list_part_null;
-- should be ok
execute pstmt_def_insert(null);
drop table pc_list_part_1;
-- should be ok
execute pstmt_def_insert(1);
drop table pc_list_parted, pc_list_part_null;
deallocate pstmt_def_insert;

-- Test plan_cache_mode

create table test_mode (a int);
insert into test_mode select 1 from generate_series(1,10000) union all select 2;
create index on test_mode (a);
analyze test_mode;

prepare test_mode_pp (int) as select count(*) from test_mode where a = $1;

-- up to 5 executions, custom plan is used
explain (costs off) execute test_mode_pp(2);

-- force generic plan
set plan_cache_mode to force_generic_plan;
explain (costs off) execute test_mode_pp(2);

-- get to generic plan by 5 executions
set plan_cache_mode to auto;
execute test_mode_pp(1); -- 1x
execute test_mode_pp(1); -- 2x
execute test_mode_pp(1); -- 3x
execute test_mode_pp(1); -- 4x
execute test_mode_pp(1); -- 5x

-- we should now get a really bad plan
explain (costs off) execute test_mode_pp(2);

-- but we can force a custom plan
set plan_cache_mode to force_custom_plan;
explain (costs off) execute test_mode_pp(2);

drop table test_mode;

--
-- Test correctness of prepare/execute statement after fixing the direct dispatch issue 
--

create table test_prepare_sql_value_function (a int, b timestamp);
prepare test_sql_value_function as select count(*) from test_prepare_sql_value_function where b < current_timestamp and a < $1;

execute test_sql_value_function(1); -- 1x
execute test_sql_value_function(1); -- 2x
execute test_sql_value_function(1); -- 3x
execute test_sql_value_function(1); -- 4x
execute test_sql_value_function(1); -- 5x
execute test_sql_value_function(1); -- 6x
execute test_sql_value_function(1); -- 7x

insert into test_prepare_sql_value_function values (1, current_timestamp);

-- should return one row
execute test_sql_value_function(100);

-- should return one row
select count(*) from test_prepare_sql_value_function where b < current_timestamp and a < 100;

drop table test_prepare_sql_value_function;
