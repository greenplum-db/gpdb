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
