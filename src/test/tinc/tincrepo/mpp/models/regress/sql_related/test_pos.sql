-- 
-- @description test sql test case: basic
-- @created 2012-07-10 12:00:00
-- @modified 2012-07-10 12:00:02
-- @tags basic
-- @product_version gpdb: main
-- @dbname gptest

-- start_ignore
drop table if exists foo;
-- end_ignore

create table foo (a int);
insert into foo select generate_series(1,9);
select * from foo order by a;
drop table foo;
