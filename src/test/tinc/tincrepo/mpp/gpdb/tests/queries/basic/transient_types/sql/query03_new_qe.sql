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
