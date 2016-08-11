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

drop schema transient_types;
