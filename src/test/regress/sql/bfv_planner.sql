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


-- Test inheritance planning, when a SubPlan is duplicated for different
-- child tables.

create table r (a int) distributed by (a);
create table p (a int, b int) distributed by (a);
create table p2 (a int, b int) inherits (p) distributed by (b);

insert into r values (3);
insert into p select a, b from generate_series(1,3) a, generate_series(1,3) b;

delete from p where b = 1 or (b=2 and a in (select r.a from r));
select * from p;

delete from p where b = 1 or (b=2 and a in (select b from r));
select * from p;


-- Test planning of IS NOT FALSE. We used treat "(a = b) IS NOT FALSE" as
-- hash joinable, and create a plan with a hash join on "(a = b)". That
-- was wrong, because NULLs are treated differently.
create table booltest (b bool);
insert into booltest values ('t');
insert into booltest values (null);
select * from booltest a, booltest b where (a.b = b.b) is not false;

-- Lossy index qual, used as a partial index predicate, and same column is
-- used in FOR SHARE. Once upon a time, this happened to tickle a bug in the
-- planner at one point.
create table tstest (t tsvector);
create index i_tstest on tstest using gist (t) WHERE t @@ 'bar';
insert into tstest values ('foo');
insert into tstest values ('bar');

set enable_bitmapscan =off;
set enable_seqscan =off;
select * from tstest where t @@ 'bar' for share of tstest;


-- Stable (and volatile) functions need to be re-evaluated on every
-- execution of a prepared statement. There used to be a bug, where
-- they were evaluated once at planning time or at first execution,
-- and the same value was incorrectly reused on subsequent executions.
create function stabletestfunc() returns integer as $$
begin
  raise notice 'stabletestfunc executed';
  return 123;
end;
$$ language plpgsql stable;

create table stabletesttab (id integer);

insert into stabletesttab values (1);
insert into stabletesttab values (1000);

-- This might evaluate the function, for cost estimate purposes. That's
-- not of importance for this test.
prepare myprep as select * from stabletesttab where id < stabletestfunc();

-- Check that the stable function should be re-executed on every execution of the prepared statetement.
execute myprep;
execute myprep;
execute myprep;


-- Test agg on top of join subquery on partition table with ORDER-BY clause
CREATE TABLE bfv_planner_t1 (a int, b int, c int) distributed by (c);
CREATE TABLE bfv_planner_t2 (f int,g int) DISTRIBUTED BY (f) PARTITION BY RANGE(g)
(
PARTITION "201612" START (1) END (10)
);
insert into bfv_planner_t1 values(1,2,3), (2,3,4), (3,4,5);
insert into bfv_planner_t2 values(3,1), (4,2), (5,2);

select count(*) from
(select a,b,c from bfv_planner_t1 order by c) T1
join
(select f,g from bfv_planner_t2) T2
on
T1.a=T2.g and T1.c=T2.f;


-- This produced a "could not find pathkey item to sort" error at one point.
-- The problem was that we stripped out column b from the SubqueryScan's
-- target list, as it's not needed in the final result, but we tried to
-- maintain the ordering (a,b) in the Gather Motion node, which would have
-- required column b to be present, at least as a junk column.
create table bfv_planner_t3 (a int4, b int4);
select a from (select * from bfv_planner_t3 order by a, b) as x limit 1;


-- start_ignore
drop table if exists bfv_planner_x;
drop table if exists testbadsql;
drop table if exists bfv_planner_foo;
drop table if exists testmedian;
drop table if exists bfv_planner_t1;
drop table if exists bfv_planner_t2;
-- end_ignore
