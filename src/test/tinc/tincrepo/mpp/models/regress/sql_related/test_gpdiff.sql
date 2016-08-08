-- 
-- @description test sql test case: gpdiff
-- @created 2012-07-11 12:00:00
-- @modified 2012-07-11 12:00:02
-- @tags basic
-- @product_version gpdb: main
-- @dbname gptest

-- start_ignore
drop table if exists foo;
-- end_ignore

create table foo (a int, b int);
insert into foo select i,i+1 from generate_series(1,8)i;
explain select * from foo;
select * from foo;
drop table foo;
drop table if exists foo;
