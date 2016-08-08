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
