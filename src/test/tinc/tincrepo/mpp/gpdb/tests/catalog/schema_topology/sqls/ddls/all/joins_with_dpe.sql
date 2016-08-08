-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Joins with dpe

--start_ignore
DROP TABLE IF EXISTS foo1;

DROP TABLE IF EXISTS foo2;

--end_ignore

create table foo1 (i int, j varchar(10)) 
partition by list(j)
(partition p1 values('1'), partition p2 values('2'), partition p3 values('3'), partition p4 values('4'), partition p5 values('5'),partition p0 values('0'));

insert into foo1 select i , i%5 || '' from generate_series(1,100) i;

create table foo2 (i int, j varchar(10));
insert into foo2 select i , i ||'' from generate_series(1,5) i;

analyze foo1;
analyze foo2;

explain select count(*) from foo1,foo2 where foo1.j = foo2.j;

select count(*) from foo1,foo2 where foo1.j = foo2.j;
