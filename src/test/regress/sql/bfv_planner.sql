--start_ignore
drop table if exists testbadsql;
drop table if exists bfv_planner_x;
drop table if exists bfv_planner_foo;
--end_ignore
CREATE TABLE testbadsql(id int);
CREATE TABLE bfv_planner_x(i integer);
CREATE TABLE bfv_planner_foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

--
-- Test unexpected internal error (execQual.c:4413) when using subquery+window function+union in 4.2.6.x
--

-- Q1
select * from
(SELECT  MIN(id) OVER () minid FROM testbadsql
UNION
SELECT  MIN(id) OVER () minid FROM testbadsql
) tmp
where tmp.minid=123;

-- Q2
select * from
(SELECT  MIN(id) OVER () minid FROM testbadsql
UNION
SELECT 1
) tmp
where tmp.minid=123;

-- Q3
select * from
(SELECT  MIN(id) OVER () minid FROM testbadsql) tmp
where tmp.minid=123;

-- Q4
SELECT * from (
  SELECT max(i) over () as w from bfv_planner_x Union Select 1 as w)
as bfv_planner_foo where w > 0;

--
-- Test query when using median function with count(*)
--

--start_ignore
drop table if exists testmedian;
--end_ignore
CREATE TABLE testmedian
(
  a character(2) NOT NULL,
  b character varying(8) NOT NULL,
  c character varying(8) NOT NULL,
  value1 double precision,
  value2 double precision
)
WITH (
  OIDS=FALSE
)
DISTRIBUTED BY (b,c);

insert into testmedian
select i, i, i, i, i
from  (select * from generate_series(1, 99) i ) a ;

-- Test with count()
select median(value1), count(*)
from  testmedian
where c ='55'
group by b, c, value2;

-- Test with varying ORDER-BY clause
select median(value1), count(*)
from  testmedian
where c ='25'
group by a, b, c, value2
order by c,b;

-- Test with varying ORDER-BY clause
select median(value1), count(*)
from  testmedian
where c ='55'
group by b,c,value1
order by b, c, value1;

-- Test with sum()
select median(value1), sum(value2)
from  testmedian
where c ='55'
group by b, c, value2;

-- Test with min()
select median(value1), min(c)
from  testmedian
where c ='55'
group by b, c, value2;

-- Test with varying GROUP-BY clause
select median(value1), count(*)
from  testmedian
where c ='55'
group by b;

-- Test with varying GROUP-BY clause
select median(value1), count(*)
from  testmedian
where c ='55'
group by b,c;

-- Test with varying GROUP-BY clause
select median(value1), count(*)
from  testmedian
where c ='55'
group by c,b;

-- Test with varying GROUP-BY clause
select median(value1), count(*)
from  testmedian
where c ='55'
group by b,c,value1;

-- Test with varying ORDER-BY clause
select median(value1), count(*)
from  testmedian
where c ='25'
group by b, value1
order by b;

-- Test with varying ORDER-BY clause
select median(value1), count(*)
from  testmedian
where c ='25'
group by b, c, value2
order by b,c;

-- start_ignore
drop table if exists bfv_planner_x;
drop table if exists testbadsql;
drop table if exists bfv_planner_foo;
drop table if exists testmedian;
-- end_ignore
