-- copied from //tincrepo/private/kodunh/query/limit/sql/execMain.sql
-- 
-- @product_version gpdb: [4.2.0.0-9.9.99.99] , hawq: [1.1.5.0-9.9.99.99]

drop table if exists test;
create table test (a integer);
insert into test select a from generate_series(1,100) a;

set log_min_messages=debug5;
EXPLAIN ANALYZE select count(*) from test;

set debug_print_slice_table=on;
select count(*) from test;

--FOR UPDATE/FOR SHARE
select * from test order by a limit 1 for update;

select * from test order by a limit 1 for share;

--returning clause
insert into test values (1) returning *;


