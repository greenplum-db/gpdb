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
