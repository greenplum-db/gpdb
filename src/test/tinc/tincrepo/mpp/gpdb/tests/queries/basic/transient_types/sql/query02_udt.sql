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
