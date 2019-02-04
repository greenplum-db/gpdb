-- Memory consumption of operators 

-- start_ignore
create schema memconsumption;
set search_path to memconsumption;
-- end_ignore

create table test (i int, j int);

set explain_memory_verbosity=detail;
set execute_pruned_plan=on;

insert into test select i, i % 100 from generate_series(1,1000) as i;

-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function sum_owner_consumption(query text, owner text) returns int as
$$
import re
rv = plpy.execute('EXPLAIN ANALYZE '+ query)
search_text = owner
total_consumption = 0
count = 0
comp_regex = re.compile("[^0-9]+(\d+)\/(\d+).+")
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        print search_text
        m = comp_regex.match(cur_line)
        if m is not None:
            count = count + 1
            total_consumption = total_consumption + int(m.group(2))
return total_consumption
$$
language plpythonu;

select sum_owner_consumption('SELECT t1.i, t2.j FROM test as t1 join test as t2 on t1.i = t2.j', 'X_Alien') = 0;

set execute_pruned_plan=off;
select sum_owner_consumption('SELECT t1.i, t2.j FROM test as t1 join test as t2 on t1.i = t2.j', 'X_Alien') > 0;

create or replace function has_account_type(query text, search_text text) returns int as
$$
import re
rv = plpy.execute('EXPLAIN ANALYZE '+ query)
comp_regex = re.compile("^\s+%s" % search_text)
count = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    m = comp_regex.match(cur_line)
    if m is not None:
        count = count + 1
return count
$$
language plpythonu;

-- Create functions that will generate nested SQL executors
CREATE OR REPLACE FUNCTION simple_plpgsql_function(int) RETURNS int AS $$
 DECLARE RESULT int;
BEGIN
 SELECT count(*) FROM pg_class INTO RESULT;
 RETURN RESULT + $1;
END;
$$ LANGUAGE plpgsql NO SQL;


CREATE OR REPLACE FUNCTION simple_sql_function(argument int) RETURNS bigint AS $$
SELECT count(*) + argument FROM pg_class;
$$ LANGUAGE SQL STRICT VOLATILE;

-- Create a table with tuples only on one segement
CREATE TABLE all_tuples_on_seg0(i int);
INSERT INTO all_tuples_on_seg0 VALUES (2), (2), (2);
SELECT gp_segment_id, count(*) FROM all_tuples_on_seg0 GROUP BY 1;

-- The X_NestedExecutor account is only created if we create an executor.
-- Because all the tuples in all_tuples_on_seg0 are on seg0, only seg0 should
-- create the X_NestedExecutor account.
set explain_memory_verbosity to detail;
-- We expect that only seg0 will create an X_NestedExecutor account, so this
-- should return '1'
select has_account_type('select simple_sql_function(i) from all_tuples_on_seg0', 'X_NestedExecutor');
-- Each node will create a 'main' executor account, so we expect that this
-- will return '3', one per Query Executor.
select has_account_type('select simple_sql_function(i) from all_tuples_on_seg0', 'Executor');
-- Same as above, this should return '1'
select has_account_type('select simple_plpgsql_function(i) from all_tuples_on_seg0', 'X_NestedExecutor');
-- Same as above, this should return '3'
select has_account_type('select simple_plpgsql_function(i) from all_tuples_on_seg0', 'Executor');

-- After setting explain_memory_verbosity to 'debug', the X_NestedExecutor
-- account will no longer be created. Instead, every time we evaluate a code
-- block in an sql or PL/pgSQL function, it will create a new executor account.
-- There are three tuples in all_tuples_on_seg0, so we should see three more
-- Executor accounts than when we had the explain_memory_verbosity guc set to
-- 'detail'.
set explain_memory_verbosity to debug;
-- We expect this will be '0'
select has_account_type('select simple_sql_function(i) from all_tuples_on_seg0', 'X_NestedExecutor');
-- Because there are three tuples in all_tuples_on_seg0, we expect to see three
-- additional 'Executor' accounts created, a total number of '6'.
select has_account_type('select simple_sql_function(i) from all_tuples_on_seg0', 'Executor');
-- Expect '0'
select has_account_type('select simple_plpgsql_function(i) from all_tuples_on_seg0', 'X_NestedExecutor');
-- Expect '6'
select has_account_type('select simple_plpgsql_function(i) from all_tuples_on_seg0', 'Executor');

-- Test X_NestedExecutor is created correctly inside multiple slice plans
set explain_memory_verbosity to detail;
-- We should see two TableScans- one per slice. Because only one segment has
-- tuples, only one segment per slice will create the 'X_NestedExecutor'
-- account. This will return '2'.
select has_account_type('select * from (select simple_sql_function(i) from all_tuples_on_seg0) a, (select simple_sql_function(i) from all_tuples_on_seg0) b', 'X_NestedExecutor');
-- There will be two slices, and each slice will create an 'Executor' account
-- for a total of '6' 'Executor' accounts.
select has_account_type('select * from (select simple_sql_function(i) from all_tuples_on_seg0) a, (select simple_sql_function(i) from all_tuples_on_seg0) b', 'Executor');


set explain_memory_verbosity to debug;
-- We don't create 'X_NestedExecutor' accounts when explain_memory_verbosity is
-- set to 'debug', so this will return '0'
select has_account_type('select * from (select simple_sql_function(i) from all_tuples_on_seg0) a, (select simple_sql_function(i) from all_tuples_on_seg0) b', 'X_NestedExecutor');
-- Two slices, each returning three tuples. For each tuple we will create an
-- 'Executor' account. We also expect one main 'Executor' account per slice, so
-- expect '12' total Executor accounts
select has_account_type('select * from (select simple_sql_function(i) from all_tuples_on_seg0) a, (select simple_sql_function(i) from all_tuples_on_seg0) b', 'Executor');

create or replace function oneoff_plan_func(a integer)
returns integer AS
$$
BEGIN
if date_part('month', now()) + 1 > a then
   return 0;
else
   return 1;
end if;
END
$$ LANGUAGE 'plpgsql' stable;

-- The oneoff_plan_func calls the stable function "now()".  Normally,
-- GPDB will agressively evaluate stable functions in the planner, at
-- the cost of being required to regenerate a plan during every
-- execution.  The benefit of this is partition elimination because
-- partition elimination is done inside the planner, by evaluating
-- stable functions we can avoid costly full table scans on tables
-- that will yield no tuples.  However, in the case of simple
-- expressions in pl/pgsql the resulting plan will never do partition
-- elimination.  In cases where we will end up with simple
-- expressions, we can prevent the planner from evaluating stable
-- functions in order for it to create a reusable plan.  A reusable
-- plan will be cached and reused for subsequent executions of
-- oneoff_plan_func.

-- Two planners, one for each evaluated statement block in oneoff_plan_func will
-- be executed on seg0.  Because seg0 is the only segment having tuples, no
-- other segment will create a plan.
select has_account_type('select oneoff_plan_func(i) from all_tuples_on_seg0', 'Planner');

-- Both plans will have been cached during previous executions will be reused,
-- therefore, we expect the output to be 0.
select has_account_type('select oneoff_plan_func(i) from all_tuples_on_seg0', 'Planner');

-- We expect only three Executor accounts, one per segment, because
-- simple expressions in pl/pgsql should not need a full executor.
select has_account_type('select oneoff_plan_func(i) from all_tuples_on_seg0', 'Executor');

-- The memory consumption for X_PartitionSelector should not go above 1 MB
-- total on all 3 segments -- It was around 7 MB before the bug in eval_part_qual was
-- fixed. Although, It was max around .07MB after the fix, but have kept a small buffer.
set explain_memory_verbosity=detail;
-- start_ignore
drop table if exists bar_part;
drop table if exists foo;
-- end_ignore
create table bar_part (a int, b int) distributed by (b) partition by range(b) (start(1) end (2) every(1));
create table foo (a int, b int) distributed by (b);
insert into bar_part select 1, 1 from generate_series(1,100000)i;
insert into foo select 1, 1 from generate_series(1,80000)i;
analyze foo;
analyze bar_part;
select sum_owner_consumption('select * from bar_part where exists (select 1 from foo where foo.b = bar_part.b and bar_part.b > 0);', 'X_PartitionSelector') between 1 and 1000000;
-- We expect, one slice in the plan for the top Gather Motion. Corresponding to
-- each segment, there will be 1 instance of the slice, thus total 3 Executor
-- accounts for 3 segments
select has_account_type('select simple_sql_function(i) from all_tuples_on_seg0', 'Executor');
-- Inserting even more data does not cause the memory usage to go high as it
-- should be independent of the tuples. However, before the fix, it increased with
-- more rows. Check the memory for X_PartitionSelector owner does not go above 1 MB.
insert into foo select 1, 1 from generate_series(1,80000)i;
select sum_owner_consumption('select * from bar_part where exists (select 1 from foo where foo.b = bar_part.b and bar_part.b > 0);', 'X_PartitionSelector') between 1 and 1000000;
insert into foo select 1, 1 from generate_series(1,80000)i;
select sum_owner_consumption('select * from bar_part where exists (select 1 from foo where foo.b = bar_part.b and bar_part.b > 0);', 'X_PartitionSelector') between 1 and 1000000;
