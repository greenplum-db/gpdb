
--
-- PLPGSQL
--
-- Testing various scenarios where plans will not be cached. 
-- MPP-16204

set client_min_messages = 'warning';
drop table if exists cache_tab cascade;
drop function if exists cache_test();
drop function if exists cache_test(int);
reset client_min_messages;

--
-- ************************************************************
-- * Repro with drop table inside a function
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--
create function cache_test() returns void as
$$
begin
	drop table if exists cache_tab;
	create table cache_tab (id int) distributed randomly;
	insert into cache_tab values (1);
end;
$$ language plpgsql MODIFIES SQL DATA;

select cache_test();

-- following should not fail. 
select cache_test();

drop table cache_tab;

drop function cache_test();

--
-- ************************************************************
-- * Repro with SELECT .. INTO inside a function
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--

create table cache_tab
(
	c1 int, 
	c2 text
) partition by range(c1)
(default partition def,
partition part1 start (int '1') end (int '10')); 

insert into cache_tab values(1, 2);
insert into cache_tab values(2, 3);
insert into cache_tab values(3, 4);

create function cache_test(id int) returns int as $$
declare 
	v_int int;
begin 
	select c1 from cache_tab where c2 = id::text INTO v_int;
	return v_int;
end;
$$ language plpgsql READS SQL DATA;

select * from cache_test(1);

-- ALTER TABLE prints a NOTICE with unpredictable temp table's name
set client_min_messages='warning';
alter table cache_tab split default partition 
start (11) inclusive
end (20) exclusive 
into (partition part2, partition def);
reset client_min_messages;

-- following should not fail. 
select * from cache_test(2);

drop table cache_tab cascade;

drop function cache_test(int);

--
-- ************************************************************
-- * Repro with drop table between executions 
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--

create table cache_tab (
	id int, 
	name varchar(50)
) distributed randomly;

insert into cache_tab values(1, 'abc');
insert into cache_tab values(2, 'def');

drop function if exists cache_test(var int);

create function cache_test(var int) returns varchar as $$
declare 
	v_name varchar(20) DEFAULT 'zzzz';
begin 
	select name from cache_tab into v_name where id = var;
	return v_name;
end;
$$ language plpgsql READS SQL DATA;

select * from cache_test(1);

drop table  if exists cache_tab;

create table cache_tab (
	id int, 
	name varchar(50)
) distributed randomly;

-- following should not fail. 
select * from cache_test(2);

drop table cache_tab;

drop function cache_test(int);

--
-- ************************************************************
-- * Repro with return cursor 
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--

create table cache_tab (
	c1 int, 
	c2 int, 
	c3 int
) distributed randomly;
 
insert into cache_tab values(1, 2, 100);

insert into cache_tab values(2, 3, 200);

insert into cache_tab values(3, 4, 300);

create function cache_test(refcursor) returns refcursor as $$
begin 
	open $1 for select * from cache_tab;
	return $1;
end;
$$
language plpgsql READS SQL DATA;

begin;
select cache_test('refcursor');
fetch all in refcursor;
commit;

drop table if exists  cache_tab; 

create table cache_tab (
	c1 int, 
	c2 int, 
	c3 int
) distributed randomly;


begin;
select cache_test('refcursor');
fetch all in refcursor;
commit;

drop table cache_tab; 

drop function cache_test(refcursor);

--
-- ************************************************************
-- * Repro with fetch cursor
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--

create table cache_tab(
	c1 int, 
	c2 int, 
	c3 int
) distributed randomly; 

insert into cache_tab values(1, 2, 100);

insert into cache_tab values(1, 2, 100);

create function cache_test(var int) returns int as $$
declare 
	cur refcursor;
	res int;
	total_res int default 0;
begin
	open cur for select c2 from cache_tab where c1 = var; 
	fetch cur into res;
	while res is not null 
	loop
  		total_res := total_res + res;
  		fetch cur into res;
	end loop;
	return total_res;
end;
$$ language plpgsql READS SQL DATA;

select cache_test(1); 

drop table  if exists cache_tab; 

create table cache_tab(
	c1 int, 
	c2 int, 
	c3 int
) distributed randomly; 

insert into cache_tab values(1, 2, 100);

-- following should not fail
select cache_test(1);

drop table cache_tab; 

drop function cache_test(int);

--
-- ************************************************************
-- * Repro with function planned on segments
-- *    - plan should be cached
-- ************************************************************
--

create table cache_tab
(
	c1 int, 
	c2 text
) partition by range(c1)
(default partition def,
partition part1 start (int '1') end (int '10')); 

insert into cache_tab values(1, 'foo1');

create function cache_test() returns int as $$
declare 
	v_temp varchar(10); 
begin 
	select into v_temp hastriggers from pg_tables;
	if v_temp is not null
	then
		return 1;
	else
		return 0;
	end if;
end;
$$ language plpgsql READS SQL DATA;
   
select * from cache_tab where c1 = cache_test(); 

select * from cache_tab where c1 = cache_test();

drop table cache_tab;

drop function cache_test();

--
-- ************************************************************
-- * Block statement execution
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--

create table cache_tab
(
	c1 int, 
	c2 int
) partition by range(c2)
(default partition def,
partition part1 start (int '1') end (int '10')); 

insert into cache_tab values(0, 100);

insert into cache_tab values(1, 100);

insert into cache_tab values(2, 100);

create function cache_test(key int) returns int as $$ 
declare 
	v_int int;
	v_res int default 0;
begin 
loop
	select c1 from cache_tab into v_int where c2 = key;
	if found then
		return v_res;
	end if;
	if v_int != 0 then
		v_res := v_res + v_int;
	end if;
end loop;
end;
$$ language plpgsql READS SQL DATA; 

select cache_test(100); 

-- ALTER TABLE prints a NOTICE with unpredictable temp table's name
set client_min_messages='warning';
alter table cache_tab split default partition 
start (11) inclusive
end (20) exclusive 
into (partition part2, partition def);
reset client_min_messages;

select cache_test(100); 

drop table cache_tab cascade; 

drop function cache_test(int);

--
-- ************************************************************
-- * Repro with PERFORM 
-- *    - Multiple executions should not raise an error 
-- ************************************************************
--

create table cache_tab
(
	c1 int, 
	c2 int
) partition by range(c2)
(default partition def,
partition part1 start (int '1') end (int '10')); 

insert into cache_tab values(1, 100);

insert into cache_tab values(2, 100);

insert into cache_tab values(3, 100);

create function cache_test() returns void AS $$
begin
	perform c1 from cache_tab;
end;
$$ language plpgsql READS SQL DATA;

select cache_test();

drop table if exists  cache_tab; 

create table cache_tab
(
	c1 int, 
	c2 int
) partition by range(c2)
(default partition def,
partition part1 start (int '1') end (int '10')); 

insert into cache_tab values(1, 100);

select cache_test();

drop table cache_tab; 

drop function cache_test();

--
-- ************************************************************
-- * example with table functions
-- *
-- ************************************************************
--

create table cache_tab
(
	a int, 
	b int
) distributed randomly;

insert into cache_tab values(1, 100);
insert into cache_tab values(2, 200);

drop function if exists get_cache_tab();

create function get_cache_tab() returns setof cache_tab as $$
	select * from cache_tab where a = 1;
$$ language sql READS SQL DATA;

create function cache_test() returns setof integer as
$$
declare 
	r integer;
begin
	for r IN select a from get_cache_tab() 
	loop
		return next r;
	end loop;
	return;
end
$$ language plpgsql READS SQL DATA;

select cache_test();

drop function if exists get_cache_tab();

create function get_cache_tab() returns setof cache_tab as $$
  select * from cache_tab where a = 2;
$$ language sql READS SQL DATA;


-- plan should not be cached, returns different results
select cache_test();

drop table cache_tab cascade;

drop function if exists get_cache_tab() ;

drop function if exists cache_test();

-- ************************************************************
-- * an example with CTAS
-- * 	multiple executions should not raise an error
-- ************************************************************
create table cache_tab(id int, data text);

insert into cache_tab values(1, 'abc');

insert into cache_tab values(2, 'abc');

insert into cache_tab values(3, 'abcd');

create or replace function cache_test() returns void as 
$$
begin
	drop table if exists cache_temp;
	create table cache_temp as select * from cache_tab distributed randomly;
end
$$ language plpgsql MODIFIES SQL DATA; 

select cache_test();

select * from cache_temp;

drop table cache_tab;

create table cache_tab(id int, data text);

insert into cache_tab values(1, 'abcde');

-- should not raise an error 
select cache_test();

-- should return 1 row 
select * from cache_temp;

drop table cache_tab cascade; 

drop function cache_test();

drop table cache_temp;

--
-- ************************************************************
-- * recursive functions
-- ************************************************************
--

create table cache_tab(c1 int, c2 int) distributed randomly; 

drop function if exists cache_test(count int); 
create function cache_test(count int) returns int as $$
begin 
	if $1 <= 0 then 
		return $1;
  	else 
		insert into cache_tab values($1, $1);
  	end if; 
  	return cache_test($1-1);
end; 
$$ language plpgsql MODIFIES SQL DATA;

select cache_test(5);
-- should return 5 rows
select * from cache_tab;

drop table if exists cache_tab;

create table cache_tab(c1 int, c2 int) distributed randomly; 

select cache_test(5);
-- should return 5 rows
select * from cache_tab;

drop function cache_test(count int); 

--- another example with recursive functions 
create function cache_test(count int) returns int as $$
begin 
	if $1 <= 0 then 
		return $1;
  	else 
		drop table if exists cache_tab;
		create table cache_tab(c1 int, c2 int) distributed randomly; 
		insert into cache_tab values($1, $1);
  	end if; 
  	return cache_test($1-1);
end; 
$$ language plpgsql MODIFIES SQL DATA;

select cache_test(5);
-- should return 1 row
select * from cache_tab;

drop table if exists cache_tab;

drop function cache_test(count int) cascade;

-- ************************************************************
-- * A function that queries a table that's dropped and recreated
-- ************************************************************
--
-- This used to fail on GPDB 4.3, but works after the PostgreSQL 8.3 merge,
-- thanks to upstream plan cache invalidation. (The old GPDB code didn't
-- force plans to be recomputed in the same transaction, only across
-- transactions. There was a GUC called gp_plpgsql_clear_cache_always that
-- you could set, and made this work, though. But that's no longer needed).

create table cache_tab (t text);
insert into cache_tab values ('b');

create function cache_test(p text) returns integer as
$$
begin
  return (select count(*) from cache_tab where t = p);
end;
$$ language plpgsql;

BEGIN;

-- Run the function. This caches the plan for the select inside the
-- function.
select cache_test('b');

-- Drop and re-create the table
drop table cache_tab;
create table cache_tab (t text);
insert into cache_tab values ('b');

-- Re-run the function.
select cache_test('b');

COMMIT;

drop table cache_tab;
drop function cache_test(text);

-- ************************************************************
-- * A function that calls another function, and the other function is
-- * dropped and recreated.
-- ************************************************************
--
-- This depends on plan cache invalidation support added in PostgreSQL 8.4.

-- Create a function, and another function that calls the first one.
create or replace function get_dummy_string(t text) returns text as $$
begin
  return 'foo ' || t;
end;
$$ language plpgsql;

create or replace function cache_test(t text) returns text as
$$
begin
  return get_dummy_string(t);
end;
$$ language plpgsql;

-- Run the function, to warm the plan cache with the function
-- call to get_dummy_string().
select cache_test('');

-- Drop and re-create get_dummy_string() function.
drop function get_dummy_string(text);
create or replace function get_dummy_string(t text) returns text as $$
begin
  return 'bar ' || t;
end;
$$ language plpgsql;

-- Re-run the function
select cache_test('');

drop function get_dummy_string(text);
drop function cache_test(text);

-- ************************************************************
-- * A function that calls another function, and the other function is
-- * dropped and recreated.
-- ************************************************************

-- Create a function, and another function that calls the first one.
create or replace function get_dummy_string(t text) returns text as $$
begin
  return 'foo ' || t;
end;
$$ language plpgsql;

create or replace function cache_test(t text) returns text as
$$
begin
  return get_dummy_string(t);
end;
$$ language plpgsql;


-- Run the function, to warm the plan cache with the function
-- call to get_dummy_string().
select cache_test('');

-- Also run the function as part of a query so that the function
-- is executed in segments rather than the master. (Without ORCA.
-- With ORCA, the query is planned differently and runs on the
-- master anyway).
create temporary table cache_tab (t text);
insert into cache_tab values ('b');

select cache_test(t) from cache_tab;

-- Drop and re-create get_dummy_string() function.
drop function get_dummy_string(text);
create or replace function get_dummy_string(t text) returns text as $$
begin
  return 'bar' || t;
end;
$$ language plpgsql;

-- Re-run the function
select cache_test('');
select cache_test(t) from cache_tab;

drop function get_dummy_string(text);
drop function cache_test(text);
drop table cache_tab;

-- ************************************************************
-- * A function that calls another function, and the other function
-- * is dropped and recreated. Same as previous tests, but the
-- * function is executed IMMUTABLE, to test that plan cache
-- * invalidation also works when the function is inlined.
-- ************************************************************
--
-- To make sure that plan invalidation works also when the function
-- is inlined.

-- Create a function, and another function that calls the first one.
create or replace function get_dummy_string(t text) returns text as $$
begin
  return 'foo ' || t;
end;
$$ language plpgsql IMMUTABLE;

create or replace function cache_test(t text) returns text as
$$
begin
  return get_dummy_string(t);
end;
$$ language plpgsql IMMUTABLE;

create temporary table cache_tab (t text);
insert into cache_tab values ('b');

-- Run the function, to warm the plan cache.
select cache_test('');
select cache_test(t) from cache_tab;

-- Drop and re-create get_dummy_string() function.
drop function get_dummy_string(text);
create or replace function get_dummy_string(t text) returns text as $$
begin
  return 'bar' || t;
end;
$$ language plpgsql;

-- Re-run the function
select cache_test('');
select cache_test(t) from cache_tab;

drop function get_dummy_string(text);
drop function cache_test(text);
drop table cache_tab;


--
-- Test that a query is re-planned every time, if the plan can use partition
-- elimination.
--
-- Starting with PostgreSQL 9.2 (merged into GPDB in GPDB version 6), we create
-- a "specific" plan for a query, on the first five calls. The specific plan
-- makes use of the exact parameter values supplied, and can use e.g. partition
-- elimination and constant-folding using the parameter values. On the fifth
-- call, the planner also generates a "generic" plan, which doesn't do those
-- things and which would be correct with any parameter values. If the generic
-- plan seems as cheap as the specific plans, then switch to using the generic
-- plan. The generic plan is cached and used on all subsequent invocations.
--
-- That's a crude heuristic, but it's better than the old behavior, where we
-- always used generic plans. The heuristic should at least catch the common
-- cases when partition elimination is helpful, and that's what we test here.
--
-- The point of this test is to test that we *don't* switch to using a generic
-- plan, when the specialized plans are better. The test function includes
-- a query that benefits from partition elimination, based on the function's
-- parameter. We execute it several times, and verify that we don't switch to
-- a generic plan that doesn't do partition elimination.

-- Create a partitioned test table with a few partitions, and one row in each
-- partition. Use a dummy distribution key, so that all the data resides on a
-- single segment; that makes the output order of the NOTICEs printed in the
-- test function deterministic.
create table cache_tab
(
  c1 int,
  c2 int,
  distkey int
) distributed by (distkey) partition by range(c1) (start (1) end(11) every(1));

insert into cache_tab select g, g, 0 from generate_series(1, 10) g;

-- A helper function that prints the parameter as a NOTICE. Claim a very
-- small cost, to encourage the planner to evaluate this function first,
-- before other quals. We use these NOTICEs to detect which rows, and hence
-- which partitions, were scanned.
--
-- We use a temporary sequence to distinguish between two calls with the same
-- value in the expected output; gpdiff masks out duplicated NOTICEs, the
-- sequence makes them different.
create sequence notice_value_seq;
create or replace function pg_temp.notice_value(t text) returns bool as
$$
begin
  raise notice 'notice_value called %: %', nextval('notice_value_seq'), t;
  return true;
end;
$$ language plpgsql VOLATILE cost 0.0001;

-- The test function.
create or replace function pg_temp.cache_test(param integer) returns integer as
$$
declare
  result integer;
  cur refcursor;
begin
  -- We want to detect whether partition elimination was used. We do that by using
  -- the notice_value() function in the qual. If partition elimination is used, we
  -- should only see NOTICEs for values in the target partition. This is a bit fragile,
  -- it relies on the fact that the planner chooses to evalues the notice_value(c2) qual
  -- before the other quals. To make sure that that happens, run a "control" test first,
  -- which cannot use partition elimination. This should print 10 NOTICEs, one for each
  -- row.
  alter sequence notice_value_seq restart;
  raise notice 'control';
  open cur for select c1 from cache_tab where pg_temp.notice_value('control') and c2 = param;
  fetch cur into result;
  close cur;
  raise notice 'control returned: %', result;

  -- And now the actual test, which should use partition elimination, and print only
  -- one NOTICE, for the only row in the matching partition.
  raise notice 'test';
  open cur for select c1 from cache_tab where pg_temp.notice_value('test ' || c2) and c1 = param;
  fetch cur into result;
  close cur;

  return result;
end;
$$ language plpgsql;

-- Repeat the test a several times, to see if we switch to generic plans.
-- We should not, because the specialized plans can do partition elimination
-- and are therefore much cheaper.
select pg_temp.cache_test(1);
select pg_temp.cache_test(2);
select pg_temp.cache_test(3);
select pg_temp.cache_test(4);
select pg_temp.cache_test(5);
select pg_temp.cache_test(6);
select pg_temp.cache_test(7);
select pg_temp.cache_test(8);
select pg_temp.cache_test(9);
select pg_temp.cache_test(10);

drop table cache_tab;
