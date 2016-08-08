-- 
-- @created 2013-04-18 12:00:00
-- @modified 2013-04-18 12:00:00
-- @tags storage catalog transaction
-- @description DTM race condition leads to catalog inconsistencies during concurrent workload
-- @concurrency 4 
-- @iterations 100

-- COUNT 1 --
create table a_new(x int, y int);
alter table a_new rename to a;
drop table a;
---------------------------------------
-- COUNT 2 --
create table a_new(x int, y int);
alter table a_new rename to a;
drop table a;
--------------------------------
-- COUNT 3 --
create table a_new(x int, y int);
alter table a_new rename to a;
drop table a;
