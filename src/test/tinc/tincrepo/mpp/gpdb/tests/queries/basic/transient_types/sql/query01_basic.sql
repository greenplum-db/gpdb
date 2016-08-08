-- prepare a distributed table
drop table if exists t;
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

