create schema transient_types;
set search_path='transient_types';

-- prepare a distributed table
create table t as select i as id from generate_series(1,8) i; 

-- test row
select row(), row(1, 2), row(1, 'a', 'abc', 1.5) from t;

-- test function
create or replace function foo(in int, out int, out int) as
$$
select $1, $1;
$$ language sql;

select row(id, 1), foo(id) from t;

-- cleanup
drop table t;
drop function foo(int);

-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,8) i; 

-- create type
drop type if exists myudt;
create type myudt as (i int, c char);

-- test row
select row((1,'a')::myudt), row((2,'b')::myudt, 2) from t;

-- test function
create or replace function foo(in int, out int, out myudt) as
$$
select $1, ($1, 't')::myudt;
$$ language sql;

select row(id, 1), foo(id) from t;

--cleanup
drop function foo(int);
drop type myudt;
drop table t;
-- Test when old QE got released, all types will be dispatched to new QE.

-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,8) i; 

-- set QE release timeout 10ms
set gp_vmem_idle_resource_timeout=10;

-- query 1 
select row(), row(1, 2), row(1, 'a', 'abc', 1.5) from t;
-- sleep 1000ms
select pg_sleep(1);
-- query 2 
select row(1, 2, 3, 4, 5, 6), row(), row(1, 2, 3, 4, 5), row(1, 2), row(1, 2, 3, 4), row(1, 'a', 'abc', 1.5) from t;
-- Utility statement won't need to dispatch transient types. So QE may have transient type which is not mapped to QD.

-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,8) i; 

-- 2 types on QD, but not on QE.
select row(1, row(1,2), 3), row(1, 2, 3, 4);

-- 1 another type on both QD and QE, but not mapped since it's not dispatched.
create or replace function foo(in int, out int, out int) as
$$
select $1, $1; 
$$ language sql;

-- dispatch 3 types to QE.
select foo(id) from t;

--cleanup
drop function foo(int);
drop table t;
-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,8) i; 

-- commit
begin;
select row(), row(1, 2), row(1, 'a', 'abc', 1.5) from t;
commit;

-- abort
begin;
select row(), row(1, row(1, 2)), row(1, 2, 3) from t;
abort;

-- subtrans
begin;
select row(1, 2) from t;
savepoint s1;
select row(1, 2, 3, 'a'), row(1, 2), row(1, 2, 3); 
select row(1, 'b', 3, 'a'), row(1, 2, 3, 4, 5, 6), row(1, 2, 3, 4) from t; 
rollback to savepoint s1;
savepoint s2;
select row(), row(1), row(1, 'a'), row(1, 2, 3);
rollback to savepoint s2;
select row(), row(1), row('a', 'b'), row(1, 2, 3, 4) from t;
commit;


-- cleanup
drop table t;
-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,100000) i; 

-- cursor
begin;
declare foo cursor for select row(id, 1),id from t order by id;
savepoint x;
fetch from foo;
rollback to x;
fetch ABSOLUTE 100000 from foo;
abort;

truncate table t;
select row(id, 1) from t;

-- cleanup
drop table t;
-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,8) i; 

create or replace function foo() returns void as $$
begin
    EXECUTE 'SELECT row(id, 2) from t';
    RETURN;
end
$$ language 'plpgsql';

select foo(); 

-- cleanup
drop function foo();
drop table t;

-- This test case use UDF assign_new_record() to generate a new record type in cache
-- for each tuple, these types should be sync to receiver. 
-- test function
create or replace function assign_new_record()
returns SETOF record as '@abs_builddir@/regress@DLSUFFIX@',
'assign_new_record' LANGUAGE C VOLATILE;

-- transfer record types via motion incrementally
select assign_new_record() from gp_dist_random('gp_id');

-- cleanup
drop function assign_new_record();

-- Test cases for record type nested in array 
drop table if exists t_array_record;
create table t_array_record as select i as c1, i * 2 as c2 from generate_series(1,10) i;
select c1, array[row(c1,array[row(2,3),row(c2,5,array[c1],'abc')])] from t_array_record order by c1;
drop table t_array_record;

-- Test the case that new record type is created during execution stage, which makes
-- QE have more transient types than QD.
drop table if exists t_record_qe;
create table t_record_qe as select f1 from generate_series(1,10) f1;
select q from (select max(f1) as max from t_record_qe group by f1 order by f1) q order by max;
drop table t_record_qe;

-- Customer reported case which used to segment fault.
DROP TABLE IF EXISTS typemod_init ;
CREATE TABLE typemod_init ( varchar_col character varying(7)); 
INSERT INTO typemod_init VALUES ('A');
SELECT C FROM ( SELECT B.varchar_col FROM ( SELECT A.varchar_col FROM typemod_init A) B GROUP BY B.varchar_col) C;
DROP TABLE typemod_init ;

-- Test the case that there is no fields require remapping, such as VOIDOID
DROP TABLE IF EXISTS t_nomap;
CREATE TABLE t_nomap AS SELECT f1 FROM generate_series(1,10) f1;
CREATE OR REPLACE FUNCTION foo()
RETURNS VOID
AS $$
BEGIN

END
$$
LANGUAGE plpgsql
IMMUTABLE;

SELECT q FROM (SELECT max(f1) AS max FROM t_nomap GROUP BY f1 ORDER BY f1) q ORDER BY max;
SELECT foo() FROM t_nomap;
DROP TABLE t_nomap;
DROP FUNCTION foo();

--
-- Use a transient record type in a parameter of a query that's dispatched to segments
--
-- For these tests, the QD needs to serialize a transient record type to the QE, while
-- all the previous tests were for the other direction, in Motion nodes.
--
drop table if exists t;
create table t as select i as id from generate_series(1,8) i;
insert into t values (100000);

do $$
declare
  r record;
  nestedr record;

  result record;
begin
  select 1 as i, 2 AS j into r;
  select r as nestedrec, 'foo' AS foo into nestedr;

  raise notice 'r: %', r;
  raise notice 'nestedr: %', nestedr;

  -- Reference one field of a 'record' variable.
  select * into result from t where id=r.i;
  raise notice 'result 1: %', result.id;

  -- Same with a nested 'record' variable
  select * into result from t where id > length(nestedr.nestedrec::text) + 1000;
  raise notice 'result 2: %', result.id;

  -- Reference the record variable itself.
  select * into result from t where id > length(nestedr::text) + 1000;
  raise notice 'result 3: %', result.id;
end;
$$;

drop table if exists t;

drop schema transient_types;
