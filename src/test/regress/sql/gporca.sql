--
-- ORCA tests
--

-- show version
SELECT count(*) from gp_opt_version();

-- Mask out Log & timestamp for orca message that has feature not supported.
-- start_matchsubs
-- m/^LOG.*\"Feature/
-- s/^LOG.*\"Feature/\"Feature/
-- end_matchsubs

-- fix the number of segments for Orca
set optimizer_segments = 3;

set optimizer_enable_master_only_queries = on;
-- master only tables

create schema orca;
-- start_ignore
GRANT ALL ON SCHEMA orca TO PUBLIC;
SET search_path to orca, public;
-- end_ignore

create table orca.r();
set allow_system_table_mods=true;
delete from gp_distribution_policy where localoid='orca.r'::regclass;
reset allow_system_table_mods;

alter table orca.r add column a int;
alter table orca.r add column b int;

insert into orca.r select i, i/3 from generate_series(1,20) i;

create table orca.s();
set allow_system_table_mods=true;
delete from gp_distribution_policy where localoid='orca.s'::regclass;
reset allow_system_table_mods;
alter table orca.s add column c int;
alter table orca.s add column d int;

insert into orca.s select i, i/2 from generate_series(1,30) i;

set optimizer_log=on;
set optimizer_enable_indexjoin=on;
set optimizer_trace_fallback = on;
-- expected fall back to the planner
select sum(distinct a), count(distinct b) from orca.r;

select * from orca.r;
select * from orca.r, orca.s where r.a=s.c;
select * from orca.r, orca.s where r.a<s.c+1 or r.a>s.c;
select sum(r.a) from orca.r;
select count(*) from orca.r;
select a, b from orca.r, orca.s group by a,b;
select r.a+1 from orca.r;
select * from orca.r, orca.s where r.a<s.c or (r.b<s.d and r.b>s.c);
select case when r.a<s.c then r.a<s.c else r.a<s.c end from orca.r, orca.s;
select case r.b<s.c when true then r.b else s.c end from orca.r, orca.s where r.a = s.d;
select * from orca.r limit 100;
select * from orca.r limit 10 offset 9;
select * from orca.r offset 10;
select sqrt(r.a) from orca.r;
select pow(r.b,r.a) from orca.r;
select b from orca.r group by b having  count(*) > 2;
select b from orca.r group by b having  count(*) <= avg(a) + (select count(*) from orca.s where s.c = r.b);
select sum(a) from orca.r group by b having count(*) > 2 order by b+1;
select sum(a) from orca.r group by b having count(*) > 2 order by b+1;

-- constants

select 0.001::numeric from orca.r;
select NULL::text, NULL::int from orca.r;
select 'helloworld'::text, 'helloworld2'::varchar from orca.r;
select 129::bigint, 5623::int, 45::smallint from orca.r;

select 0.001::numeric from orca.r;
select NULL::text, NULL::int from orca.r;
select 'helloworld'::text, 'helloworld2'::varchar from orca.r;
select 129::bigint, 5623::int, 45::smallint from orca.r;

--  distributed tables

create table orca.foo (x1 int, x2 int, x3 int);
create table orca.bar1 (x1 int, x2 int, x3 int) distributed randomly;
create table orca.bar2 (x1 int, x2 int, x3 int) distributed randomly;

insert into orca.foo select i,i+1,i+2 from generate_series(1,10) i;

insert into orca.bar1 select i,i+1,i+2 from generate_series(1,20) i;
insert into orca.bar2 select i,i+1,i+2 from generate_series(1,30) i;

-- produces result node

select x2 from orca.foo where x1 in (select x2 from orca.bar1);
select 1;
SELECT 1 AS one FROM orca.foo having 1 < 2;
SELECT generate_series(1,5) AS one FROM orca.foo having 1 < 2;
SELECT 1 AS one FROM orca.foo group by x1 having 1 < 2;
SELECT x1 AS one FROM orca.foo having 1 < 2;

-- distinct clause
select distinct 1, null;
select distinct 1, null from orca.foo;
select distinct 1, sum(x1) from orca.foo;
select distinct x1, rank() over(order by x1) from (select x1 from orca.foo order by x1) x; --order none
select distinct x1, sum(x3) from orca.foo group by x1,x2;
select distinct s from (select sum(x2) s from orca.foo group by x1) x;
select * from orca.foo a where a.x1 = (select distinct sum(b.x1)+avg(b.x1) sa from orca.bar1 b group by b.x3 order by sa limit 1);
select distinct a.x1 from orca.foo a where a.x1 <= (select distinct sum(b.x1)+avg(b.x1) sa from orca.bar1 b group by b.x3 order by sa limit 1) order by 1;
select * from orca.foo a where a.x1 = (select distinct b.x1 from orca.bar1 b where b.x1=a.x1 limit 1);

-- with clause
with cte1 as (select * from orca.foo) select a.x1+1 from (select * from cte1) a group by a.x1;
select count(*)+1 from orca.bar1 b where b.x1 < any (with cte1 as (select * from orca.foo) select a.x1+1 from (select * from cte1) a group by a.x1);
select count(*)+1 from orca.bar1 b where b.x1 < any (with cte1 as (select * from orca.foo) select a.x1 from (select * from cte1) a group by a.x1);
select count(*)+1 from orca.bar1 b where b.x1 < any (with cte1 as (select * from orca.foo) select a.x1 from cte1 a group by a.x1);
with cte1 as (select * from orca.foo) select count(*)+1 from cte1 a where a.x1 < any (with cte2 as (select * from cte1 b where b.x1 > 10) select c.x1 from (select * from cte2) c group by c.x1);
with cte1 as (select * from orca.foo) select count(*)+1 from cte1 a where a.x1 < any (with cte2 as (select * from cte1 b where b.x1 > 10) select c.x1+1 from (select * from cte2) c group by c.x1);
with x as (select * from orca.foo) select count(*) from (select * from x) y where y.x1 <= (select count(*) from x);
with x as (select * from orca.foo) select count(*)+1 from (select * from x) y where y.x1 <= (select count(*) from x);
with x as (select * from orca.foo) select count(*) from (select * from x) y where y.x1 < (with z as (select * from x) select count(*) from z);

-- outer references
select count(*)+1 from orca.foo x where x.x1 > (select count(*)+1 from orca.bar1 y where y.x1 = x.x2);
select count(*)+1 from orca.foo x where x.x1 > (select count(*) from orca.bar1 y where y.x1 = x.x2);
select count(*) from orca.foo x where x.x1 > (select count(*)+1 from orca.bar1 y where y.x1 = x.x2);

-- result node with one time filter and filter
explain select case when bar1.x2 = bar2.x2 then coalesce((select 1 from orca.foo where bar1.x2 = bar2.x2 and bar1.x2 = random() and foo.x2 = bar2.x2),0) else 1 end as col1, bar1.x1
from orca.bar1 inner join orca.bar2 on (bar1.x2 = bar2.x2) order by bar1.x1; 
select case when bar1.x2 = bar2.x2 then coalesce((select 1 from orca.foo where bar1.x2 = bar2.x2 and bar1.x2 = random() and foo.x2 = bar2.x2),0) else 1 end as col1, bar1.x1
from orca.bar1 inner join orca.bar2 on (bar1.x2 = bar2.x2) order by bar1.x1; 

drop table orca.r cascade;
create table orca.r(a int, b int) distributed by (a);
create unique index r_a on orca.r(a);
create index r_b on orca.r(b);

insert into orca.r select i, i%3 from generate_series(1,20) i;

drop table orca.s;
create table orca.s(c int, d int) distributed by (c);

insert into orca.s select i%7, i%2 from generate_series(1,30) i;

analyze orca.r;
analyze orca.s;

select * from orca.r, orca.s where r.a=s.c;

-- Materialize node
select * from orca.r, orca.s where r.a<s.c+1 or r.a>s.c;

-- empty target list
select r.* from orca.r, orca.s where s.c=2;

create table orca.m();
alter table orca.m add column a int;
alter table orca.m add column b int;

create table orca.m1();
alter table orca.m1 add column a int;
alter table orca.m1 add column b int;

insert into orca.m select i-1, i%2 from generate_series(1,35) i;
insert into orca.m1 select i-2, i%3 from generate_series(1,25) i;

insert into orca.r values (null, 1);

-- join types
select r.a, s.c from orca.r left outer join orca.s on(r.a=s.c);
select r.a, s.c from orca.r left outer join orca.s on(r.a=s.c and r.a=r.b and s.c=s.d) order by r.a,s.c;
select r.a, s.c from orca.r left outer join orca.s on(r.a=s.c) where s.d > 2 or s.d is null order by r.a;
select r.a, s.c from orca.r right outer join orca.s on(r.a=s.c);
select * from orca.r where exists (select * from orca.s where s.c=r.a + 2);
select * from orca.r where exists (select * from orca.s where s.c=r.b);
select * from orca.m where m.a not in (select a from orca.m1 where a=5);
select * from orca.m where m.a not in (select a from orca.m1);
select * from orca.m where m.a in (select a from orca.m1 where m1.a-1 = m.b);

-- enable_hashjoin=off; enable_mergejoin=on
select 1 from orca.m, orca.m1 where m.a = m1.a and m.b!=m1.b;

-- plan.qual vs hashclauses/join quals:
select * from orca.r left outer join orca.s on (r.a=s.c and r.b<s.d) where s.d is null;
-- select * from orca.r m full outer join orca.r m1 on (m.a=m1.a) where m.a is null;

-- explain Hash Join with 'IS NOT DISTINCT FROM' join condition
-- force_explain
explain  select * from orca.r, orca.s where r.a is not distinct from s.c;

-- explain Hash Join with equality join condition
-- force_explain
explain select * from orca.r, orca.s where r.a = s.c;

-- sort
select * from orca.r join orca.s on(r.a=s.c) order by r.a, s.d;
select * from orca.r join orca.s on(r.a=s.c) order by r.a, s.d limit 10;
select * from orca.r join orca.s on(r.a=s.c) order by r.a + 5, s.d limit 10;

-- group by
select 1 from orca.m group by a+b;

-- join with const table
select * from orca.r where a = (select 1);

-- union with const table
select * from ((select a as x from orca.r) union (select 1 as x )) as foo order by x;

insert into orca.m values (1,-1), (1,2), (1,1);

-- computed columns
select a,a,a+b from orca.m;
select a,a+b,a+b from orca.m;

-- func expr
select * from orca.m where a=abs(b);

-- grouping sets

select a,b,count(*) from orca.m group by grouping sets ((a), (a,b));
select b,count(*) from orca.m group by grouping sets ((a), (a,b));
select a,count(*) from orca.m group by grouping sets ((a), (a,b));
select a,count(*) from orca.m group by grouping sets ((a), (b));
select a,b,count(*) from orca.m group by rollup(a, b);
select a,b,count(*) from orca.m group by rollup((a),(a,b)) order by 1,2,3;
select count(*) from orca.m group by ();
select a, count(*) from orca.r group by (), a;
select a, count(*) from orca.r group by grouping sets ((),(a));

select a, b, count(*) c from orca.r group by grouping sets ((),(a), (a,b)) order by b,a,c;
select a, count(*) c from orca.r group by grouping sets ((),(a), (a,b)) order by b,a,c;

select 1 from orca.r group by ();
select a,1 from orca.r group by rollup(a);

-- arrays
select array[array[a,b]], array[b] from orca.r;

-- setops
select a, b from orca.m union select b,a from orca.m;
SELECT a from orca.m UNION ALL select b from orca.m UNION ALL select a+b from orca.m group by 1;

drop table if exists orca.foo;
create table orca.foo(a int, b int, c int, d int);

drop table if exists orca.bar;
create table orca.bar(a int, b int, c int);

insert into orca.foo select i, i%2, i%4, i-1 from generate_series(1,40)i;
insert into orca.bar select i, i%3, i%2 from generate_series(1,30)i;

-- distinct operation
SELECT distinct a, b from orca.foo;
SELECT distinct foo.a, bar.b from orca.foo, orca.bar where foo.b = bar.a;
SELECT distinct a, b from orca.foo;
SELECT distinct a, count(*) from orca.foo group by a;
SELECT distinct foo.a, bar.b, sum(bar.c+foo.c) from orca.foo, orca.bar where foo.b = bar.a group by foo.a, bar.b;
SELECT distinct a, count(*) from orca.foo group by a;
SELECT distinct foo.a, bar.b from orca.foo, orca.bar where foo.b = bar.a;
SELECT distinct foo.a, bar.b, sum(bar.c+foo.c) from orca.foo, orca.bar where foo.b = bar.a group by foo.a, bar.b;

SELECT distinct a, b from orca.foo;
SELECT distinct a, count(*) from orca.foo group by a;
SELECT distinct foo.a, bar.b from orca.foo, orca.bar where foo.b = bar.a;
SELECT distinct foo.a, bar.b, sum(bar.c+foo.c) from orca.foo, orca.bar where foo.b = bar.a group by foo.a, bar.b;

-- window operations
select row_number() over() from orca.foo order by 1;
select rank() over(partition by b order by count(*)/sum(a)) from orca.foo group by a, b order by 1;
select row_number() over(order by foo.a) from orca.foo inner join orca.bar using(b) group by foo.a, bar.b, bar.a;
select 1+row_number() over(order by foo.a+bar.a) from orca.foo inner join orca.bar using(b);
select row_number() over(order by foo.a+ bar.a)/count(*) from orca.foo inner join orca.bar using(b) group by foo.a, bar.a, bar.b;
select count(*) over(partition by b order by a range between 1 preceding and (select count(*) from orca.bar) following) from orca.foo;
select a+1, rank() over(partition by b+1 order by a+1) from orca.foo order by 1, 2;
select a , sum(a) over (order by a range '1'::float8 preceding) from orca.r order by 1,2;
select a, b, floor(avg(b) over(order by a desc, b desc rows between unbounded preceding and unbounded following)) as avg, dense_rank() over (order by a) from orca.r order by 1,2,3,4;
select lead(a) over(order by a) from orca.r order by 1;
select lag(c,d) over(order by c,d) from orca.s order by 1;
select lead(c,c+d,1000) over(order by c,d) from orca.s order by 1;

-- test normalization of window functions
create table orca_w1(a int, b int);
create table orca_w2(a int, b int);
create table orca_w3(a int, b text);

insert into orca_w1 select i, i from generate_series(1, 3) i;
insert into orca_w2 select i, i from generate_series(2, 4) i;
insert into orca_w3 select i, i from generate_series(3, 5) i;

-- outer ref in subquery in target list and window func in target list
select (select b from orca_w3 where a = orca_w1.a) as one, row_number() over(partition by orca_w1.a) as two from orca_w2, orca_w1;

-- aggref in subquery with window func in target list
select orca_w1.a, (select sum(orca_w2.a) from orca_w2 where orca_w1.b = orca_w2.b), count(*), rank() over (order by orca_w1.b) from orca_w1 group by orca_w1.a, orca_w1.b order by orca_w1.a;

-- window function inside subquery inside target list with outer ref
select orca_w1.a, (select rank() over (order by orca_w1.b) from orca_w2 where orca_w1.b = orca_w2.b), count(*) from orca_w1 group by orca_w1.a, orca_w1.b order by orca_w1.a;

-- window function with empty partition clause inside subquery inside target list with outer ref
select (select rank() over() from orca_w3 where a = orca_w1.a) as one, row_number() over(partition by orca_w1.a) as two from orca_w1, orca_w2;

-- window function in IN clause
select (select a from orca_w3 where a = orca_w1.a) as one from orca_w1 where orca_w1.a IN (select rank() over(partition by orca_w1.a) + 1 from orca_w1, orca_w2);

-- window function in subquery inside target list with outer ref in partition clause
select (select rank() over(partition by orca_w2.a) from orca_w3 where a = orca_w1.a) as one, row_number() over(partition by orca_w1.a) as two from orca_w1, orca_w2 order by orca_w1.a;

-- window function in subquery inside target list with outer ref in order clause
select (select rank() over(order by orca_w2.a) from orca_w3 where a = orca_w1.a) as one, row_number() over(partition by orca_w1.a) as two from orca_w1, orca_w2 order by orca_w1.a;

-- window function with outer ref in arguments
select (select sum(orca_w1.a + a) over(order by b) + 1 from orca_w2 where orca_w1.a = orca_w2.a) from orca_w1 order by orca_w1.a;

-- window function with outer ref in window clause and arguments 
select (select sum(orca_w1.a + a) over(order by b + orca_w1.a) + 1 from orca_w2 where orca_w1.a = orca_w2.a) from orca_w1 order by orca_w1.a;


-- cte
with x as (select a, b from orca.r)
select rank() over(partition by a, case when b = 0 then a+b end order by b asc) as rank_within_parent from x order by a desc ,case when a+b = 0 then a end ,b;

-- alias
select foo.d from orca.foo full join orca.bar on (foo.d = bar.a) group by d;
select 1 as v from orca.foo full join orca.bar on (foo.d = bar.a) group by d;
select * from orca.r where a in (select count(*)+1 as v from orca.foo full join orca.bar on (foo.d = bar.a) group by d+r.b);
select * from orca.r where r.a in (select d+r.b+1 as v from orca.foo full join orca.bar on (foo.d = bar.a) group by d+r.b) order by r.a, r.b;

drop table if exists orca.rcte;
create table orca.rcte(a int, b int, c int);

insert into orca.rcte select i, i%2, i%3 from generate_series(1,40)i;

with x as (select * from orca.rcte where a < 10) select * from x x1, x x2;
with x as (select * from orca.rcte where a < 10) select * from x x1, x x2 where x2.a = x1.b;
with x as (select * from orca.rcte where a < 10) select a from x union all select b from x;
with x as (select * from orca.rcte where a < 10) select * from x x1 where x1.b = any (select x2.a from x x2 group by x2.a);
with x as (select * from orca.rcte where a < 10) select * from x x1 where x1.b = all (select x2.a from x x2 group by x2.a);
with x as (select * from orca.rcte where a < 10) select * from x x1, x x2, x x3 where x2.a = x1.b and x3.b = x2.b                                                                                   ;
with x as (select * from orca.rcte where a < 10) select * from x x2 where x2.b < (select avg(b) from x x1);
with x as (select r.a from orca.r, orca.s  where r.a < 10 and s.d < 10 and r.a = s.d) select * from x x1, x x2;
with x as (select r.a from orca.r, orca.s  where r.a < 10 and s.c < 10 and r.a = s.c) select * from x x1, x x2;
with x as (select * from orca.rcte where a < 10) (select a from x x2) union all (select max(a) from x x1);
with x as (select * from orca.r) select * from x order by a;
-- with x as (select * from orca.rcte where a < 10) select * from x x1, x x2 where x2.a = x1.b limit 1;

-- correlated execution
select (select 1 union select 2);
select (select generate_series(1,5));
select (select a from orca.foo inner1 where inner1.a=outer1.a  union select b from orca.foo inner2 where inner2.b=outer1.b) from orca.foo outer1;
select (select generate_series(1,1)) as series;
select generate_series(1,5);
select a, c from orca.r, orca.s where a = any (select c) order by a, c limit 10;
select a, c from orca.r, orca.s where a = (select c) order by a, c limit 10;
select a, c from orca.r, orca.s where a  not in  (select c) order by a, c limit 10;
select a, c from orca.r, orca.s where a  = any  (select c from orca.r) order by a, c limit 10;
select a, c from orca.r, orca.s where a  <> all (select c) order by a, c limit 10;
select a, (select (select (select c from orca.s where a=c group by c))) as subq from orca.r order by a;
with v as (select a,b from orca.r, orca.s where a=c)  select c from orca.s group by c having count(*) not in (select b from v where a=c) order by c;

CREATE TABLE orca.onek (
unique1 int4,
unique2 int4,
two int4,
four int4,
ten int4,
twenty int4,
hundred int4,
thousand int4,
twothousand int4,
fivethous int4,
tenthous int4,
odd int4,
even int4,
stringu1 name,
stringu2 name,
string4 name
);

insert into orca.onek values (931,1,1,3,1,11,1,31,131,431,931,2,3,'VJAAAA','BAAAAA','HHHHxx');
insert into orca.onek values (714,2,0,2,4,14,4,14,114,214,714,8,9,'MBAAAA','CAAAAA','OOOOxx');
insert into orca.onek values (711,3,1,3,1,11,1,11,111,211,711,2,3,'JBAAAA','DAAAAA','VVVVxx');
insert into orca.onek values (883,4,1,3,3,3,3,83,83,383,883,6,7,'ZHAAAA','EAAAAA','AAAAxx');
insert into orca.onek values (439,5,1,3,9,19,9,39,39,439,439,18,19,'XQAAAA','FAAAAA','HHHHxx');
insert into orca.onek values (670,6,0,2,0,10,0,70,70,170,670,0,1,'UZAAAA','GAAAAA','OOOOxx');
insert into orca.onek values (543,7,1,3,3,3,3,43,143,43,543,6,7,'XUAAAA','HAAAAA','VVVVxx');

select ten, sum(distinct four) from orca.onek a
group by ten
having exists (select 1 from orca.onek b where sum(distinct a.four) = b.four);

-- indexes on partitioned tables
create table orca.pp(a int) partition by range(a)(partition pp1 start(1) end(10));
create index pp_a on orca.pp(a);

-- list partition tests

-- test homogeneous partitions
drop table if exists orca.t;

create table orca.t ( a int, b char(2), to_be_drop int, c int, d char(2), e int)
distributed by (a)
partition by list(d) (partition part1 values('a'), partition part2 values('b'));

insert into orca.t
	select i, i::char(2), i, i, case when i%2 = 0 then 'a' else 'b' end, i
	from generate_series(1,100) i;

select * from orca.t order by 1, 2, 3, 4, 5, 6 limit 4;

alter table orca.t drop column to_be_drop;

select * from orca.t order by 1, 2, 3, 4, 5 limit 4;

insert into orca.t (d, a) values('a', 0);
insert into orca.t (a, d) values(0, 'b');

select * from orca.t order by 1, 2, 3, 4, 5 limit 4;

create table orca.multilevel_p (a int, b int)
partition by range(a)
subpartition by range(a)
subpartition template
(
subpartition sp1 start(0) end(10) every (5),
subpartition sp2 start(10) end(200) every(50)
)
(partition aa start(0) end (100) every (50),
partition bb start(100) end(200) every (50) );

insert into orca.multilevel_p values (1,1), (100,200);
select * from orca.multilevel_p;


-- test appendonly
drop table if exists orca.t;

create table orca.t ( a int, b char(2), to_be_drop int, c int, d char(2), e int)
distributed by (a)
partition by list(d) (partition part1 values('a') with (appendonly=true, compresslevel=5, orientation=column), partition part2 values('b') with (appendonly=true, compresslevel=5, orientation=column));

insert into orca.t
	select i, i::char(2), i, i, case when i%2 = 0 then 'a' else 'b' end, i
	from generate_series(1,100) i;

select * from orca.t order by 1, 2, 3, 4, 5, 6 limit 4;

alter table orca.t drop column to_be_drop;

select * from orca.t order by 1, 2, 3, 4, 5 limit 4;

insert into orca.t
	select i, i::char(2), i, case when i%2 = 0 then 'a' else 'b' end, i
	from generate_series(1,100) i;

select * from orca.t order by 1, 2, 3, 4, 5 limit 4;


-- test heterogeneous partitions
drop table if exists orca.t;

create table orca.t ( timest character varying(6), user_id numeric(16,0) not null, to_be_drop char(5), tag1 char(5), tag2 char(5))
distributed by (user_id)
partition by list (timest) (partition part201203 values('201203') with (appendonly=true, compresslevel=5, orientation=column), partition part201204 values('201204') with (appendonly=true, compresslevel=5, orientation=row), partition part201205 values('201205'));

insert into orca.t values('201203',0,'drop', 'tag1','tag2');

alter table orca.t drop column to_be_drop;

alter table orca.t add partition part201206 values('201206') with (appendonly=true, compresslevel=5, orientation=column);
alter table orca.t add partition part201207 values('201207') with (appendonly=true, compresslevel=5, orientation=row);
alter table orca.t add partition part201208 values('201208');

insert into orca.t values('201203',1,'tag1','tag2');
insert into orca.t values('201204',2,'tag1','tag2');
insert into orca.t values('201205',1,'tag1','tag2');
insert into orca.t values('201206',2,'tag1','tag2');
insert into orca.t values('201207',1,'tag1','tag2');
insert into orca.t values('201208',2,'tag1','tag2');

-- test projections
select * from orca.t order by 1,2;

-- test EXPLAIN support of partition selection nodes, while we're at it.
explain select * from orca.t order by 1,2;

select tag2, tag1 from orca.t order by 1, 2;;

select tag1, user_id from orca.t order by 1, 2;

insert into orca.t(user_id, timest, tag2) values(3, '201208','tag2');

select * from orca.t order by 1, 2;

-- test heterogeneous indexes with constant expression evaluation
drop table if exists orca.t_date;

create table orca.t_date ( timest date, user_id numeric(16,0) not null, tag1 char(5), tag2 char(5))
distributed by (user_id)
partition by list (timest) (partition part201203 values('01-03-2012'::date), partition part201204 values('01-04-2012'::date), partition part201205 values('01-05-2012'::date));
create index user_id_idx on orca.t_date_1_prt_part201203(user_id);

alter table orca.t_date add partition part201206 values('01-06-2012'::date);
alter table orca.t_date add partition part201207 values('01-07-2012'::date);
alter table orca.t_date add partition part201208 values('01-08-2012'::date);

insert into orca.t_date values('01-03-2012'::date,0,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,1,'tag1','tag2');
insert into orca.t_date values('01-04-2012'::date,2,'tag1','tag2');
insert into orca.t_date values('01-05-2012'::date,1,'tag1','tag2');
insert into orca.t_date values('01-06-2012'::date,2,'tag1','tag2');
insert into orca.t_date values('01-07-2012'::date,1,'tag1','tag2');
insert into orca.t_date values('01-08-2012'::date,2,'tag1','tag2');

insert into orca.t_date values('01-03-2012'::date,2,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,3,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,4,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,5,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,6,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,7,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,8,'tag1','tag2');
insert into orca.t_date values('01-03-2012'::date,9,'tag1','tag2');

set optimizer_enable_partial_index=on;
set optimizer_enable_space_pruning=off;
set optimizer_enable_constant_expression_evaluation=on;
set optimizer_enumerate_plans=on;
set optimizer_plan_id = 2;
-- start_ignore
analyze orca.t_date;
-- end_ignore
explain select * from orca.t_date where user_id=9;
select * from orca.t_date where user_id=9;

reset optimizer_enable_space_pruning;
set optimizer_enumerate_plans=off;
set optimizer_enable_constant_expression_evaluation=off;

drop table if exists orca.t_text;
drop index if exists orca.user_id_idx;
create table orca.t_text ( timest date, user_id numeric(16,0) not null, tag1 char(5), tag2 char(5))
distributed by (user_id)
partition by list(tag1) (partition partgood values('good'::text), partition partbad values('bad'::text), partition partugly values('ugly'::text));

create index user_id_idx on orca.t_text_1_prt_partgood(user_id);

insert into orca.t_text values('01-03-2012'::date,0,'good','tag2');
insert into orca.t_text values('01-03-2012'::date,1,'bad','tag2');
insert into orca.t_text values('01-04-2012'::date,2,'ugly','tag2');
insert into orca.t_text values('01-05-2012'::date,1,'good','tag2');
insert into orca.t_text values('01-06-2012'::date,2,'bad','tag2');
insert into orca.t_text values('01-07-2012'::date,1,'ugly','tag2');
insert into orca.t_text values('01-08-2012'::date,2,'good','tag2');

insert into orca.t_text values('01-03-2012'::date,2,'bad','tag2');
insert into orca.t_text values('01-03-2012'::date,3,'ugly','tag2');
insert into orca.t_text values('01-03-2012'::date,4,'good','tag2');
insert into orca.t_text values('01-03-2012'::date,5,'bad','tag2');
insert into orca.t_text values('01-03-2012'::date,6,'ugly','tag2');
insert into orca.t_text values('01-03-2012'::date,7,'good','tag2');
insert into orca.t_text values('01-03-2012'::date,8,'bad','tag2');
insert into orca.t_text values('01-03-2012'::date,9,'ugly','tag2');

set optimizer_enable_space_pruning=off;
set optimizer_enable_constant_expression_evaluation=on;
set optimizer_enumerate_plans=on;
set optimizer_plan_id = 2;
-- start_ignore
analyze orca.t_text;
-- end_ignore
explain select * from orca.t_text where user_id=9;
select * from orca.t_text where user_id=9;

reset optimizer_enable_space_pruning;
set optimizer_enumerate_plans=off;
reset optimizer_enable_constant_expression_evaluation;
reset optimizer_enable_partial_index;

-- test that constant expression evaluation works with integers
drop table if exists orca.t_ceeval_ints;

create table orca.t_ceeval_ints(user_id numeric(16,0), category_id int, tag1 char(5), tag2 char(5))
distributed by (user_id)
partition by list (category_id)
	(partition part100 values('100') , partition part101 values('101'), partition part103 values('102'));
create index user_id_ceeval_ints on orca.t_ceeval_ints_1_prt_part101(user_id);

insert into orca.t_ceeval_ints values(1, 100, 'tag1', 'tag2');
insert into orca.t_ceeval_ints values(2, 100, 'tag1', 'tag2');
insert into orca.t_ceeval_ints values(3, 100, 'tag1', 'tag2');
insert into orca.t_ceeval_ints values(4, 101, 'tag1', 'tag2');
insert into orca.t_ceeval_ints values(5, 102, 'tag1', 'tag2');

set optimizer_enable_partial_index=on;
set optimizer_enable_space_pruning=off;
set optimizer_enable_constant_expression_evaluation=on;
set optimizer_use_external_constant_expression_evaluation_for_ints = on;
set optimizer_enumerate_plans=on;
set optimizer_plan_id = 2;
-- start_ignore
analyze orca.t_ceeval_ints;
-- end_ignore
explain select * from orca.t_ceeval_ints where user_id=4;
select * from orca.t_ceeval_ints where user_id=4;

reset optimizer_enable_space_pruning;
reset optimizer_enumerate_plans;
reset optimizer_use_external_constant_expression_evaluation_for_ints;
reset optimizer_enable_constant_expression_evaluation;
reset optimizer_enable_partial_index;

-- test project elements in TVF

CREATE FUNCTION orca.csq_f(a int) RETURNS int AS $$ select $1 $$ LANGUAGE SQL;

CREATE TABLE orca.csq_r(a int);

INSERT INTO orca.csq_r VALUES (1);

SELECT * FROM orca.csq_r WHERE a IN (SELECT * FROM orca.csq_f(orca.csq_r.a));

-- test algebrization of having clause
drop table if exists orca.tab1;
create table orca.tab1(a int, b int, c int, d int, e int);
insert into orca.tab1 values (1,2,3,4,5);
insert into orca.tab1 values (1,2,3,4,5);
insert into orca.tab1 values (1,2,3,4,5);
select b,d from orca.tab1 group by b,d having min(distinct d)>3;
select b,d from orca.tab1 group by b,d having d>3;
select b,d from orca.tab1 group by b,d having min(distinct d)>b;

create table orca.fooh1 (a int, b int, c int);
create table orca.fooh2 (a int, b int, c int);
insert into orca.fooh1 select i%4, i%3, i from generate_series(1,20) i;
insert into orca.fooh2 select i%3, i%2, i from generate_series(1,20) i;

select sum(f1.b) from orca.fooh1 f1 group by f1.a;
select f1.a + 1 from fooh1 f1 group by f1.a+1 having sum(f1.a+1) + 1 > 20;
select 1 as one, f1.a from orca.fooh1 f1 group by f1.a having sum(f1.b) > 4;
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having 10 > (select f2.a from orca.fooh2 f2 group by f2.a having sum(f1.a) > count(*) order by f2.a limit 1) order by f1.a;
select 1 from orca.fooh1 f1 group by f1.a having 10 > (select f2.a from orca.fooh2 f2 group by f2.a having sum(f1.a) > count(*) order by f2.a limit 1) order by f1.a;
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having 10 > (select 1 from orca.fooh2 f2 group by f2.a having sum(f1.b) > count(*) order by f2.a limit 1) order by f1.a;
select 1 from orca.fooh1 f1 group by f1.a having 10 > (select 1 from orca.fooh2 f2 group by f2.a having sum(f1.b) > count(*) order by f2.a limit 1) order by f1.a;

select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having 0 = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b) > 1 order by f2.a limit 1) order by f1.a;
select 1 as one from orca.fooh1 f1 group by f1.a having 0 = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b) > 1 order by f2.a limit 1) order by f1.a;
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having 0 = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b) > 1 order by f2.a limit 1) order by f1.a;
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having 0 = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b + f1.a) > 1 order by f2.a limit 1) order by f1.a;

select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having 0 = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b + sum(f1.b)) > 1 order by f2.a limit 1) order by f1.a;
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having f1.a < (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b + 1) > f1.a order by f2.a desc limit 1) order by f1.a;
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having f1.a = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b) + 1 > f1.a order by f2.a desc limit 1);
select f1.a, 1 as one from orca.fooh1 f1 group by f1.a having f1.a = (select f2.a from orca.fooh2 f2 group by f2.a having sum(f2.b) > 1 order by f2.a limit 1);
select sum(f1.a+1)+1 from orca.fooh1 f1 group by f1.a+1;
select sum(f1.a+1)+sum(f1.a+1) from orca.fooh1 f1 group by f1.a+1;
select sum(f1.a+1)+avg(f1.a+1), sum(f1.a), sum(f1.a+1) from orca.fooh1 f1 group by f1.a+1;

--
-- test algebrization of group by clause with subqueries
--
drop table if exists foo, bar, jazz;
create table foo (a int, b int, c int);
create table bar (d int, e int, f int);
create table jazz (g int, h int, j int);

insert into foo values (1, 1, 1), (2, 2, 2), (3, 3, 3);
insert into bar values (1, 1, 1), (2, 2, 2), (3, 3, 3);
insert into jazz values (2, 2, 2);

-- subquery with outer reference with aggfunc in target list
select a, (select sum(e) from bar where foo.b = bar.f), b, count(*) from foo, jazz where foo.c = jazz.g group by b, a, h;

-- complex agg expr in subquery
select foo.a, (select (foo.a + foo.b) * count(bar.e) from bar), b, count(*) from foo group by foo.a, foo.b, foo.a + foo.b;

-- aggfunc over an outer reference in a subquery
select (select sum(foo.a + bar.d) from bar) from foo group by a, b;

-- complex expression of aggfunc over an outer reference in a subquery
select (select sum(foo.a + bar.d) + 1 from bar) from foo group by a, b;

-- aggrefs with multiple agglevelsup
select (select (select sum(foo.a + bar.d) from jazz) from bar) from foo group by a, b;

-- aggrefs with multiple agglevelsup in an expression
select (select (select sum(foo.a + bar.d) * 2 from jazz) from bar) from foo group by a, b;

-- nested group by
select (select max(f) from bar where d = 1 group by a, e) from foo group by a;

-- cte with an aggfunc of outer ref
select a, count(*), (with cte as (select min(d) dd from bar group by e) select max(a * dd) from cte) from foo group by a;

-- cte with an aggfunc of outer ref in an complex expression
select a, count(*), (with cte as (select e, min(d) as dd from bar group by e) select max(a) * sum(dd) from cte) from foo group by a;

-- subquery in group by
select max(a) from foo group by (select e from bar where bar.e = foo.a);

-- nested subquery in group by
select max(a) from foo group by (select g from jazz where foo.a = (select max(a) from foo where c = 1 group by b));

-- group by inside groupby inside group by ;S
select max(a) from foo group by (select min(g) from jazz where foo.a = (select max(g) from jazz group by h) group by h);

-- cte subquery in group by
select max(a) from foo group by b, (with cte as (select min(g) from jazz group by h) select a from cte);

-- group by subquery in order by
select * from foo order by ((select min(bar.e + 1) * 2 from bar group by foo.a) - foo.a);

-- everything in the kitchen sink
select max(b), (select foo.a * count(bar.e) from bar), (with cte as (select e, min(d) as dd from bar group by e) select max(a) * sum(dd) from cte), count(*) from foo group by foo.a, (select min(g) from jazz where foo.a = (select max(g) from jazz group by h) group by h), (with cte as (select min(g) from jazz group by h) select a from cte) order by ((select min(bar.e + 1) * 2 from bar group by foo.a) - foo.a);

-- complex expression in group by & targetlist
select b + (a+1) from foo group by b, a+1;

-- subselects inside aggs
SELECT  foo.b+1, avg (( SELECT bar.f FROM bar WHERE bar.d = foo.b)) AS t FROM foo GROUP BY foo.b;

SELECT foo.b+1, sum( 1 + (SELECT bar.f FROM bar WHERE bar.d = ANY (SELECT jazz.g FROM jazz WHERE jazz.h = foo.b))) AS t FROM foo GROUP BY foo.b;

select foo.b+1, sum((with cte as (select * from jazz) select 1 from cte where cte.h = foo.b)) as t FROM foo GROUP BY foo.b;

-- ctes inside aggs
select foo.b+1, sum((with cte as (select * from jazz) select 1 from cte cte1, cte cte2 where cte1.h = foo.b)) as t FROM foo GROUP BY foo.b;

drop table foo, bar, jazz;

create table orca.t77(C952 text) WITH (compresstype=zlib,compresslevel=2,appendonly=true,blocksize=393216,checksum=true);
insert into orca.t77 select 'text'::text;
insert into orca.t77 select 'mine'::text;
insert into orca.t77 select 'apple'::text;
insert into orca.t77 select 'orange'::text;

SELECT to_char(AVG( char_length(DT466.C952) ), '9999999.9999999'), MAX( char_length(DT466.C952) ) FROM orca.t77 DT466 GROUP BY char_length(DT466.C952);

create table orca.prod9 (sale integer, prodnm varchar,price integer);
insert into orca.prod9 values (100, 'shirts', 500);
insert into orca.prod9 values (200, 'pants',800);
insert into orca.prod9 values (300, 't-shirts', 300);

-- returning product and price using Having and Group by clause

select prodnm, price from orca.prod9 GROUP BY prodnm, price HAVING price !=300;

-- analyze on tables with dropped attributes
create table orca.toanalyze(a int, b int);
insert into orca.toanalyze values (1,1), (2,2), (3,3);
alter table orca.toanalyze drop column a;
analyze orca.toanalyze;

-- union

create table orca.ur (a int, b int);
create table orca.us (c int, d int);
create table orca.ut(a int);
create table orca.uu(c int, d bigint);
insert into orca.ur values (1,1);
insert into orca.ur values (1,2);
insert into orca.ur values (2,1);
insert into orca.us values (1,3);
insert into orca.ut values (3);
insert into orca.uu values (1,3);

select * from (select a, a from orca.ur union select c, d from orca.us) x(g,h);
select * from (select a, a from orca.ur union select c, d from orca.us) x(g,h), orca.ut t where t.a = x.h;
select * from (select a, a from orca.ur union select c, d from orca.uu) x(g,h), orca.ut t where t.a = x.h;
select 1 AS two UNION select 2.2;
select 2.2 AS two UNION select 1;
select * from (select 2.2 AS two UNION select 1) x(a), (select 1.0 AS two UNION ALL select 1) y(a) where y.a = x.a;

-- window functions inside inline CTE

CREATE TABLE orca.twf1 AS SELECT i as a, i+1 as b from generate_series(1,10)i;
CREATE TABLE orca.twf2 AS SELECT i as c, i+1 as d from generate_series(1,10)i;

SET optimizer_cte_inlining_bound=1000;
SET optimizer_cte_inlining = on;

WITH CTE(a,b) AS
(SELECT a,d FROM orca.twf1, orca.twf2 WHERE a = d),
CTE1(e,f) AS
( SELECT f1.a, rank() OVER (PARTITION BY f1.b ORDER BY CTE.a) FROM orca.twf1 f1, CTE )
SELECT * FROM CTE1,CTE WHERE CTE.a = CTE1.f and CTE.a = 2 ORDER BY 1;

SET optimizer_cte_inlining = off;

-- catalog queries
select 1 from pg_class c group by c.oid limit 1;

-- CSQs
drop table if exists orca.tab1;
drop table if exists orca.tab2;
create table orca.tab1 (i, j) as select i,i%2 from generate_series(1,10) i;
create table orca.tab2 (a, b) as select 1, 2;
select * from orca.tab1 where 0 < (select count(*) from generate_series(1,i)) order by 1;
select * from orca.tab1 where i > (select b from orca.tab2);

-- subqueries
select NULL in (select 1);
select 1 in (select 1);
select 1 in (select 2);
select NULL in (select 1/0);
select 1 where 22 in (SELECT unnest(array[1,2]));
select 1 where 22 not in (SELECT unnest(array[1,2]));
select 1 where 22 in (SELECT generate_series(1,10));
select 1 where 22 not in (SELECT generate_series(1,10));

-- UDAs
CREATE FUNCTION sum_sfunc(anyelement,anyelement) returns anyelement AS 'select $1+$2' LANGUAGE SQL STRICT;
CREATE FUNCTION sum_combinefunc(anyelement,anyelement) returns anyelement AS 'select $1+$2' LANGUAGE SQL STRICT;
CREATE AGGREGATE myagg1(anyelement) (SFUNC = sum_sfunc, COMBINEFUNC = sum_combinefunc, STYPE = anyelement, INITCOND = '0');
SELECT myagg1(i) FROM orca.tab1;

CREATE FUNCTION sum_sfunc2(anyelement,anyelement,anyelement) returns anyelement AS 'select $1+$2+$3' LANGUAGE SQL STRICT;
CREATE AGGREGATE myagg2(anyelement,anyelement) (SFUNC = sum_sfunc2, STYPE = anyelement, INITCOND = '0');
SELECT myagg2(i,j) FROM orca.tab1;

CREATE FUNCTION gptfp(anyarray,anyelement) RETURNS anyarray AS 'select $1 || $2' LANGUAGE SQL;
CREATE FUNCTION gpffp(anyarray) RETURNS anyarray AS 'select $1' LANGUAGE SQL;
CREATE AGGREGATE myagg3(BASETYPE = anyelement, SFUNC = gptfp, STYPE = anyarray, FINALFUNC = gpffp, INITCOND = '{}');
CREATE TABLE array_table(f1 int, f2 int[], f3 text);
INSERT INTO array_table values(1,array[1],'a');
INSERT INTO array_table values(2,array[11],'b');
INSERT INTO array_table values(3,array[111],'c');
INSERT INTO array_table values(4,array[2],'a');
INSERT INTO array_table values(5,array[22],'b');
INSERT INTO array_table values(6,array[222],'c');
INSERT INTO array_table values(7,array[3],'a');
INSERT INTO array_table values(8,array[3],'b');
SELECT f3, myagg3(f1) from (select * from array_table order by f1 limit 10) as foo GROUP BY f3 ORDER BY f3;

-- MPP-22453: wrong result in indexscan when the indexqual compares different data types
create table mpp22453(a int, d date);
insert into mpp22453 values (1, '2012-01-01'), (2, '2012-01-02'), (3, '2012-12-31');
create index mpp22453_idx on mpp22453(d);
set optimizer_enable_tablescan = off;
select * from mpp22453 where d > date '2012-01-31' + interval '1 day' ;
select * from mpp22453 where d > '2012-02-01';
reset optimizer_enable_tablescan;

-- MPP-22791: SIGSEGV when querying a table with default partition only
create table mpp22791(a int, b int) partition by range(b) (default partition d);
insert into mpp22791 values (1, 1), (2, 2), (3, 3);
select * from mpp22791 where b > 1;
select * from mpp22791 where b <= 3;

-- MPP-20713, MPP-20714, MPP-20738: Const table get with a filter
select 1 as x where 1 in (2, 3);

-- MPP-23081: keys of partitioned tables
create table orca.p1(a int) partition by range(a)(partition pp1 start(1) end(10), partition pp2 start(10) end(20));
insert into orca.p1 select * from generate_series(2,15);
select count(*) from (select gp_segment_id,ctid,tableoid from orca.p1 group by gp_segment_id,ctid,tableoid) as foo;

-- MPP-25194: histograms on text columns are dropped in ORCA. NDVs of these histograms should be added to NDVRemain
CREATE TABLE orca.tmp_verd_s_pp_provtabs_agt_0015_extract1 (
    uid136 character(16),
    tab_nr smallint,
    prdgrp character(5),
    bg smallint,
    typ142 smallint,
    ad_gsch smallint,
    guelt_ab date,
    guelt_bis date,
    guelt_stat text,
    verarb_ab timestamp(5) without time zone,
    u_erzeugen integer,
    grenze integer,
    erstellt_am_122 timestamp(5) without time zone,
    erstellt_am_135 timestamp(5) without time zone,
    erstellt_am_134 timestamp(5) without time zone
)
WITH (appendonly=true, compresstype=zlib) DISTRIBUTED BY (uid136);

 set allow_system_table_mods=true;

 UPDATE pg_class
 SET
         relpages = 30915::int, reltuples = 7.28661e+07::real WHERE relname = 'tmp_verd_s_pp_provtabs_agt_0015_extract1' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'xvclin');
insert into pg_statistic
values (
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  1::smallint,
  'f',
  0::real,
  17::integer,
  264682::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  1054::oid,
  1058::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.000161451,0.000107634,0.000107634,0.000107634,0.000107634,0.000107634,0.000107634,0.000107634,0.000107634,0.000107634,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{8X8#1F8V92A2025G,YFAXQ§UBF210PA0P,2IIVIE8V92A2025G,9§BP8F8V92A2025G,35A9EE8V92A2025G,§AJ2Z§9MA210PA0P,3NUQ3E8V92A2025G,F7ZD4F8V92A2025G,$WHHEO§§E210PA0P,Z6EATH2BE210PA0P,N7I28E8V92A2025G,YU0K$E$§9210PA0P,3TAI1ANIF210PA0P,P#H8BF8V92A2025G,VTQ$N$D§92A201SC,N7ZD4F8V92A2025G,77BP8F8V92A2025G,39XOXY78H210PA01,#2#OX6NHH210PA01,2DG1J#XZH210PA01,MFEG$E8V92A2025G,M0HKWNGND210PA0P,FSXI67NSA210PA0P,C1L77E8V92A2025G,01#21E8V92A2025G}'::bpchar[],
  '{§00§1HKZC210PA0P,1D90GE8V92A2025G,2ULZI6L0O210PA01,489G7L8$I210PA01,5RE8FF8V92A2025G,76NIRFNIF210PA0P,8KOMKE8V92A2025G,#9Y#GPSHB210PA0P,BDAJ#D8V92A2025G,CV9Z7IYVK210PA01,#EC5FE8V92A2025G,FQWY§O1XC210PA0P,H8HL4E8V92A2025G,INC5FE8V92A2025G,K4MX0XHCF210PA0P,LKE8FF8V92A2025G,N03G9UM2F210PA0P,OHJ$#GFZ9210PA0P,PXU3T1OTB210PA0P,RCUA45F1H210PA01,SU§FRY#QI210PA01,UABHMLSLK210PA01,VRBP8F8V92A2025G,X65#KZIDC210PA0P,YLFG§#A2G210PA0P,ZZG8H29OC210PA0P,ZZZDBCEVA210PA0P}'::bpchar[],
  NULL::bpchar[],
  NULL::bpchar[],
  NULL::bpchar[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  2::smallint,
  'f',
  0::real,
  2::integer,
  205.116::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  94::oid,
  95::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.278637,0.272448,0.0303797,0.0301106,0.0249442,0.0234373,0.0231682,0.0191319,0.0169793,0.0162527,0.0142884,0.0141539,0.0125394,0.0103329,0.0098216,0.00944488,0.00850308,0.00715766,0.0066464,0.00656567,0.00591987,0.0050588,0.00454753,0.00449372,0.0044399}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{199,12,14,5,197,11,198,8,152,299,201,153,9,13,74,179,24,202,2,213,17,195,215,16,200}'::int2[],
  '{1,5,9,12,14,24,58,80,152,195,198,199,207,302,402}'::int2[],
  NULL::int2[],
  NULL::int2[],
  NULL::int2[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  3::smallint,
  'f',
  0::real,
  6::integer,
  201.005::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  1054::oid,
  1058::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.0478164,0.0439146,0.0406856,0.0239755,0.0154186,0.0149073,0.0148804,0.0143422,0.0141808,0.0139386,0.0138848,0.0138848,0.0137502,0.0134812,0.0134004,0.0133197,0.0133197,0.013239,0.0131852,0.0130775,0.0130775,0.0130237,0.0129699,0.0129699,0.012943}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{AG023,AG032,AG999,AG022,AG--B,AG-VB,AG--C,AG-VC,AG014,AG-VA,AG036,AGT4C,AG--A,AG037,AG009,AG015,AG003,AG002,AGT3C,AG025,AG019,AGT2C,AGT1C,AG005,AG031}'::bpchar[],
  '{AG001,AG004,AG007,AG010,AG013,AG017,AG020,AG022,AG023,AG026,AG030,AG032,AG034,AG037,AG040,AG045,AG122,AG999,AG--B,AGT1C,AGT4C,AG-VB,MA017,MA--A,MK081,MKRKV}'::bpchar[],
  NULL::bpchar[],
  NULL::bpchar[],
  NULL::bpchar[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  4::smallint,
  'f',
  0::real,
  2::integer,
  34::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  94::oid,
  95::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.1135,0.112881,0.111778,0.100288,0.0895245,0.0851384,0.0821785,0.0723569,0.0565616,0.0368646,0.0309986,0.0160375,0.00621586,0.00594677,0.0050588,0.00489734,0.00487044,0.00487044,0.00468208,0.00462826,0.00460135,0.00441299,0.00438608,0.00417081,0.00414391}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1,3,2,7,9,4,5,16,10,6,8,77,12,11,14,13,22,23,64,61,24,51,53,15,54}'::int2[],
  '{1,2,3,4,5,6,7,9,10,14,16,17,53,98}'::int2[],
  NULL::int2[],
  NULL::int2[],
  NULL::int2[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  5::smallint,
  'f',
  0::real,
  2::integer,
  1::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  94::oid,
  95::oid,
  0::oid,
  0::oid,
  0::oid,
  '{1}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1}'::int2[],
  '{1}'::int2[],
  NULL::int2[],
  NULL::int2[],
  NULL::int2[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  6::smallint,
  'f',
  0::real,
  2::integer,
  2::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  94::oid,
  95::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.850227,0.149773}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1,2}'::int2[],
  '{1,2}'::int2[],
  NULL::int2[],
  NULL::int2[],
  NULL::int2[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  7::smallint,
  'f',
  0::real,
  4::integer,
  591.134::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  1093::oid,
  1095::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.26042,0.0859995,0.0709308,0.0616473,0.0567231,0.0303797,0.0109787,0.0106289,0.00990232,0.00987541,0.00979469,0.00944488,0.00820709,0.00718457,0.00626968,0.00621586,0.00616204,0.00600059,0.00586605,0.00557006,0.00516643,0.00511261,0.0050857,0.0050857,0.0047628}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1994-01-01,1997-01-01,2005-07-01,1999-01-01,2003-09-01,2000-01-01,2001-01-01,1998-10-01,2001-05-01,1999-03-01,2013-01-01,2003-01-01,2008-01-01,2004-01-01,2009-01-01,2003-02-01,1998-09-01,2000-12-01,2007-04-01,1998-08-01,1998-01-01,2003-07-01,1998-11-01,2005-02-01,1999-04-01}'::date[],
  '{1900-01-01,1994-01-01,1997-01-01,1998-05-07,1999-01-01,1999-05-01,2000-01-01,2001-03-22,2002-10-01,2003-09-01,2004-08-01,2005-07-01,2007-01-01,2008-06-01,2010-04-01,2012-07-01,2014-12-01,2015-01-01}'::date[],
  NULL::date[],
  NULL::date[],
  NULL::date[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  8::smallint,
  'f',
  0::real,
  4::integer,
  474.232::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  1093::oid,
  1095::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.52111,0.0709308,0.0591987,0.0587412,0.0148804,0.010279,0.00990232,0.00931034,0.00791109,0.00718457,0.00688857,0.00608132,0.00557006,0.0053817,0.00503189,0.00500498,0.00457444,0.004117,0.00395555,0.00390173,0.00357883,0.00352501,0.00352501,0.00344429,0.00333665}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{9999-12-31,2005-07-01,1999-01-01,2003-09-01,2004-02-01,2001-05-01,2005-02-01,1999-03-01,1998-10-01,2000-01-01,2003-01-01,1998-09-01,1998-08-01,2008-01-01,2007-04-01,2000-12-01,1999-04-01,1998-11-01,2003-02-01,1994-01-01,2002-09-01,1999-02-01,2004-01-01,1998-07-01,2003-07-01}'::date[],
  '{1994-01-01,1998-11-01,1999-01-01,1999-04-01,2001-05-01,2003-07-01,2003-09-01,2004-02-01,2005-07-01,2007-02-01,2010-03-01,9999-12-31}'::date[],
  NULL::date[],
  NULL::date[],
  NULL::date[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  9::smallint,
  'f',
  0::real,
  2::integer,
  1::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  98::oid,
  664::oid,
  0::oid,
  0::oid,
  0::oid,
  '{1}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{H}'::text[],
  '{H}'::text[],
  NULL::text[],
  NULL::text[],
  NULL::text[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  10::smallint,
  'f',
  0::real,
  8::integer,
  1::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  2060::oid,
  2062::oid,
  0::oid,
  0::oid,
  0::oid,
  '{1}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1900-01-01 00:00:00}'::timestamp[],
  '{1900-01-01 00:00:00}'::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  11::smallint,
  'f',
  1::real,
  0::integer,
  -1::real,
  0::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  0::oid,
  0::oid,
  0::oid,
  0::oid,
  0::oid,
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::int4[],
  NULL::int4[],
  NULL::int4[],
  NULL::int4[],
  NULL::int4[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  12::smallint,
  'f',
  0::real,
  4::integer,
  1::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  96::oid,
  97::oid,
  0::oid,
  0::oid,
  0::oid,
  '{1}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1}'::int4[],
  '{1}'::int4[],
  NULL::int4[],
  NULL::int4[],
  NULL::int4[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  13::smallint,
  'f',
  0.991927::real,
  8::integer,
  301.416::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  2060::oid,
  2062::oid,
  0::oid,
  0::oid,
  0::oid,
  '{8.07255e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05,2.69085e-05}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{2004-01-05 13:45:06.18894,2007-03-20 12:02:33.73888,2009-12-15 12:33:55.32684,2012-09-21 09:58:09.70321,2012-02-13 14:56:03.11625,2000-10-02 09:56:01.24836,1998-01-14 10:11:29.43055,2014-01-08 14:41:44.85935,2012-06-21 12:46:37.48899,2013-07-23 14:27:52.27322,2011-10-27 16:17:10.01694,2005-07-13 16:55:30.7964,2003-06-05 14:53:13.71932,2002-07-22 10:22:31.0967,2011-12-27 16:12:54.85765,2001-01-12 11:40:09.16207,2005-12-30 08:46:31.30943,2007-03-01 08:29:36.765,2011-06-16 09:09:43.8651,2000-12-15 14:33:29.20083,2006-04-25 13:46:46.09684,2011-06-20 16:26:23.65135,2004-01-23 12:37:06.92535,2002-03-04 10:02:08.92547,2003-08-01 10:33:57.33683}'::timestamp[],
  '{1997-12-05 10:59:43.94611,2014-11-18 08:48:18.32773}'::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  14::smallint,
  'f',
  0.329817::real,
  8::integer,
  38109.5::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  2060::oid,
  2062::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.0715766,0.0621317,0.00546242,0.0044399,0.000134542,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05,8.07255e-05}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{1997-11-25 19:05:00.83798,1998-12-29 16:19:18.50226,1998-01-28 23:01:18.41289,2000-12-21 18:03:30.34549,2003-08-28 11:14:33.26306,2003-08-28 11:14:33.2622,1999-03-20 13:04:33.24015,2003-08-28 11:14:33.22312,2003-08-28 11:14:33.21933,2003-08-28 11:14:33.22082,2003-08-28 11:14:33.21425,2003-08-28 11:14:33.22336,2003-08-28 11:14:33.2092,2003-08-28 11:14:33.20251,2003-08-28 11:14:33.22145,2003-08-28 11:14:33.26235,2003-08-28 11:14:33.26525,2003-08-28 11:14:33.23714,2003-08-28 11:14:33.26667,2003-08-28 11:14:33.23978,2003-08-28 11:14:33.21527,2003-08-28 11:14:33.2227,1999-04-16 09:01:42.92689,2003-08-28 11:14:33.21846,2003-08-28 11:14:33.24725}'::timestamp[],
  '{1997-11-25 19:05:00.83798,1998-01-05 11:57:12.19545,1998-10-14 09:06:01.87217,1998-12-29 16:19:18.50226,1999-01-18 15:01:52.77062,1999-12-28 07:57:37.93632,2001-05-16 10:55:44.78317,2003-05-23 10:32:40.1846,2003-08-28 11:14:33.23985,2004-02-04 14:01:57.60942,2005-07-26 17:01:10.98951,2005-07-26 18:41:33.09864,2006-04-25 16:52:03.49003,2008-02-18 14:17:08.58924,2010-04-19 10:16:19.03194,2012-07-23 10:47:40.65789,2014-12-05 10:59:02.25493}'::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[]
),
(
  'orca.tmp_verd_s_pp_provtabs_agt_0015_extract1'::regclass,
  15::smallint,
  'f',
  0.678255::real,
  8::integer,
  7167.15::real,
  1::smallint,
  2::smallint,
  0::smallint,
  0::smallint,
  0::smallint,
  2060::oid,
  2062::oid,
  0::oid,
  0::oid,
  0::oid,
  '{0.000376719,0.00034981,0.00034981,0.00034981,0.00034981,0.00034981,0.00034981,0.000322902,0.000322902,0.000322902,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000295993,0.000269085,0.000269085}'::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  NULL::real[],
  '{2012-09-07 14:14:23.95552,2012-09-07 14:14:36.8171,2008-12-02 09:02:19.06415,2012-03-16 15:57:15.10939,1999-06-08 13:59:56.59862,1998-10-29 14:57:47.93588,1997-07-10 09:55:34.68999,2003-03-26 11:18:44.43314,2002-02-27 15:07:42.12004,2002-02-27 15:07:13.65666,2003-03-26 11:22:07.7484,2013-11-29 13:16:25.79261,2007-09-06 09:14:10.7907,1998-10-29 14:54:04.23854,2003-04-11 07:54:56.90542,2006-03-09 15:42:27.40086,2000-05-31 10:27:52.92485,2006-01-23 17:12:44.80256,2003-01-28 10:44:17.44046,2007-11-01 15:11:21.99194,2006-03-09 15:42:56.14013,2004-03-31 10:58:47.12524,1999-06-08 14:02:11.91465,1997-07-11 14:52:47.95918,1999-06-08 13:58:15.07927}'::timestamp[],
  '{1997-07-09 10:42:54.69421,1997-09-16 10:30:42.71499,1999-06-08 14:32:08.31914,2002-02-27 15:07:13.67355,2003-04-08 16:31:58.80724,2004-05-05 10:18:01.33179,2006-03-13 16:19:59.61215,2007-09-06 09:13:41.71774,2013-11-29 13:16:37.17591,2014-12-03 09:24:25.20945}'::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[],
  NULL::timestamp[]
);

set optimizer_segments = 256;
select a.*,  b.guelt_ab as guelt_ab_b, b.guelt_bis as guelt_bis_b
from orca.tmp_verd_s_pp_provtabs_agt_0015_extract1 a
left outer join  orca.tmp_verd_s_pp_provtabs_agt_0015_extract1 b
 ON  a.uid136=b.uid136 and a.tab_nr=b.tab_nr and  a.prdgrp=b.prdgrp and a.bg=b.bg and a.typ142=b.typ142 and a.ad_gsch=b.ad_gsch
AND a.guelt_ab <= b.guelt_ab AND a.guelt_bis > b.guelt_ab
;

set optimizer_segments = 3;
set allow_system_table_mods=false;
-- Arrayref
drop table if exists orca.arrtest;
create table orca.arrtest (
 a int2[],
 b int4[][][],
 c name[],
 d text[][]
 ) DISTRIBUTED RANDOMLY;

insert into orca.arrtest (a[1:5], b[1:1][1:2][1:2], c, d)
values ('{1,2,3,4,5}', '{{{0,0},{1,2}}}', '{}', '{}');

select a[1:3], b[1][2][1], c[1], d[1][1] FROM orca.arrtest order by 1,2,3,4;

select a[b[1][2][2]] from orca.arrtest;

-- MPP-20713, MPP-20714, MPP-20738: Const table get with a filter
select 1 as x where 1 in (2, 3);

-- MPP-22918: join inner child with universal distribution
SELECT generate_series(1,10) EXCEPT SELECT 1;

-- MPP-23932: SetOp of const table and volatile function
SELECT generate_series(1,10) INTERSECT SELECT 1;

SELECT generate_series(1,10) UNION SELECT 1;
-- warning messages for missing stats
create table foo_missing_stats(a int, b int);
insert into foo_missing_stats select i, i%5 from generate_series(1,20) i;
create table bar_missing_stats(c int, d int);
insert into bar_missing_stats select i, i%8 from generate_series(1,30) i;

analyze foo_missing_stats;
analyze bar_missing_stats;

select count(*) from foo_missing_stats where a = 10;
with x as (select * from foo_missing_stats) select count(*) from x x1, x x2 where x1.a = x2.a;
with x as (select * from foo_missing_stats) select count(*) from x x1, x x2 where x1.a = x2.b;

set allow_system_table_mods=true;
delete from pg_statistic where starelid='foo_missing_stats'::regclass;
delete from pg_statistic where starelid='bar_missing_stats'::regclass;

select count(*) from foo_missing_stats where a = 10;
with x as (select * from foo_missing_stats) select count(*) from x x1, x x2 where x1.a = x2.a;
with x as (select * from foo_missing_stats) select count(*) from x x1, x x2 where x1.a = x2.b;

set optimizer_print_missing_stats = off;

-- Push components of disjunctive predicates
create table cust(cid integer, firstname text, lastname text) distributed by (cid);
create table datedim(date_sk integer, year integer, moy integer) distributed by (date_sk);
create table sales(item_sk integer, ticket_number integer, cid integer, date_sk integer, type text)
  distributed by (item_sk, ticket_number);
-- create a volatile function which should not be pushed
CREATE FUNCTION plusone(integer) RETURNS integer AS $$
BEGIN
    SELECT $1 + 1;
END;
$$ LANGUAGE plpgsql volatile;

-- force_explain
set optimizer_segments = 3;
explain
select c.cid cid,
       c.firstname firstname,
       c.lastname lastname,
       d.year dyear
from cust c, sales s, datedim d
where c.cid = s.cid and s.date_sk = d.date_sk and
      ((d.year = 2001 and lower(s.type) = 't1' and plusone(d.moy) = 5) or (d.moy = 4 and upper(s.type) = 'T2'));
reset optimizer_segments;
-- Bitmap indexes
drop table if exists orca.bm_test;
create table orca.bm_test (i int, t text);
insert into orca.bm_test select i % 10, (i % 10)::text  from generate_series(1, 100) i;
create index bm_test_idx on orca.bm_test using bitmap (i);

set optimizer_enable_bitmapscan=on;
explain select * from orca.bm_test where i=2 and t='2';
select * from orca.bm_test where i=2 and t='2';
reset optimizer_enable_bitmapscan;

-- Dynamic bitmap indexes
drop table if exists orca.bm_dyn_test;
create table orca.bm_dyn_test (i int, to_be_dropped char(5), j int, t text) distributed by (i) partition by list(j)
  (partition part0 values(0) with (appendonly=true, compresslevel=5, orientation=column),
   partition part1 values(1),
   partition part2 values(2) with (appendonly=true, compresslevel=5, orientation=row),
   partition part3 values(3), partition part4 values(4));
insert into orca.bm_dyn_test select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;
create index bm_dyn_test_idx on orca.bm_dyn_test using bitmap (i);

alter table orca.bm_dyn_test drop column to_be_dropped;
alter table orca.bm_dyn_test add partition part5 values(5);
insert into orca.bm_dyn_test values(2, 5, '2');

set optimizer_enable_bitmapscan=on;
-- start_ignore
analyze orca.bm_dyn_test;
-- end_ignore
-- gather on 1 segment because of direct dispatch
explain select * from orca.bm_dyn_test where i=2 and t='2';
select * from orca.bm_dyn_test where i=2 and t='2';

reset optimizer_enable_bitmapscan;

-- Now, create a partial index
drop table if exists orca.bm_dyn_test_onepart;
create table orca.bm_dyn_test_onepart (i int, to_be_dropped char(5), j int, t text)
  distributed by (i) partition by list(j)
  (partition part0 values(0) with (appendonly=true, compresslevel=5, orientation=column),
   partition part1 values(1),
   partition part2 values(2) with (appendonly=true, compresslevel=5, orientation=row),
   partition part3 values(3), partition part4 values(4));
insert into orca.bm_dyn_test_onepart select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;
create index bm_test_idx_part on orca.bm_dyn_test_onepart_1_prt_part2 using bitmap (i);

alter table orca.bm_dyn_test_onepart drop column to_be_dropped;
alter table orca.bm_dyn_test_onepart add partition part5 values(5);
insert into orca.bm_dyn_test_onepart values(2, 5, '2');

set optimizer_enable_bitmapscan=on;
set optimizer_enable_dynamictablescan = off;
-- start_ignore
analyze orca.bm_dyn_test_onepart;
-- end_ignore
-- gather on 1 segment because of direct dispatch
explain select * from orca.bm_dyn_test_onepart where i=2 and t='2';
select * from orca.bm_dyn_test_onepart where i=2 and t='2';

reset optimizer_enable_dynamictablescan;
reset optimizer_enable_bitmapscan;

-- Multi-level partition with dynamic bitmap indexes
create table orca.bm_dyn_test_multilvl_part (id int, year int, quarter int, region text)
distributed by (id) partition by range (year)
  subpartition by range (quarter)
    subpartition template (
      start (1) end (3) every (1) )
      subpartition by list (region)
      subpartition template (
      subpartition usa values ('usa'),
      default subpartition other_regions )
  ( start (2018) end (2020) every (1) );
insert into orca.bm_dyn_test_multilvl_part select i, 2018 + (i%2), i%2 + 1, 'usa' from generate_series(1,100)i;
create index bm_multi_test_idx_part on orca.bm_dyn_test_multilvl_part using bitmap(year);
analyze orca.bm_dyn_test_multilvl_part;
-- print name of parent index
explain select * from orca.bm_dyn_test_multilvl_part where year = 2019;
select count(*) from orca.bm_dyn_test_multilvl_part where year = 2019;

-- More BitmapTableScan & BitmapIndexScan tests

set optimizer_enable_bitmapscan=on;
create schema bm;
-- Bitmap index scan on Heterogeneous parts with dropped columns
drop table if exists bm.het_bm;
create table bm.het_bm (i int, to_be_dropped char(5), j int, t text) distributed by (i) partition by list(j)
  (partition part0 values(0) with (appendonly=true, compresslevel=5, orientation=column),
   partition part1 values(1),
   partition part2 values(2) with (appendonly=true, compresslevel=5, orientation=row),
   partition part3 values(3), partition part4 values(4));
insert into bm.het_bm select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;
create index het_bm_idx on bm.het_bm using bitmap (i);

alter table bm.het_bm drop column to_be_dropped;
alter table bm.het_bm add partition part5 values(5);
insert into bm.het_bm values(2, 5, '2');

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.het_bm where i=2 and t='2';

-- Bitmap index scan on heap parts with dropped columns
drop table if exists bm.hom_bm_heap;
create table bm.hom_bm_heap (i int, to_be_dropped char(5), j int, t text) distributed by (i) partition by list(j)
  (partition part0 values(0),
   partition part1 values(1),
   partition part2 values(2),
   partition part3 values(3),
   partition part4 values(4));
insert into bm.hom_bm_heap select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;
create index hom_bm_heap_idx on bm.hom_bm_heap using bitmap (i);

alter table bm.hom_bm_heap drop column to_be_dropped;
alter table bm.hom_bm_heap add partition part5 values(5);
insert into bm.hom_bm_heap values(2, 5, '2');

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.hom_bm_heap where i=2 and t='2';

-- Bitmap index scan on AO parts with dropped columns
drop table if exists bm.hom_bm_ao;
create table bm.hom_bm_ao (i int, to_be_dropped char(5), j int, t text)
with (appendonly=true, compresslevel=5, orientation=row)
distributed by (i) partition by list(j)
  (partition part0 values(0),
   partition part1 values(1),
   partition part2 values(2),
   partition part3 values(3),
   partition part4 values(4));
insert into bm.hom_bm_ao select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;
create index hom_bm_ao_idx on bm.hom_bm_ao using bitmap (i);

alter table bm.hom_bm_ao drop column to_be_dropped;
alter table bm.hom_bm_ao add partition part5 values(5);
insert into bm.hom_bm_ao values(2, 5, '2');

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.hom_bm_ao where i=2 and t='2';

-- Bitmap index scan on AOCO parts with dropped columns
drop table if exists bm.hom_bm_aoco;
create table bm.hom_bm_aoco (i int, to_be_dropped char(5), j int, t text)
with (appendonly=true, compresslevel=5, orientation=column)
distributed by (i) partition by list(j)
  (partition part0 values(0),
   partition part1 values(1),
   partition part2 values(2),
   partition part3 values(3),
   partition part4 values(4));
insert into bm.hom_bm_aoco select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;
create index hom_bm_aoco_idx on bm.hom_bm_aoco using bitmap (i);

alter table bm.hom_bm_aoco drop column to_be_dropped;
alter table bm.hom_bm_aoco add partition part5 values(5);
insert into bm.hom_bm_aoco values(2, 5, '2');

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.hom_bm_aoco where i=2 and t='2';

-- Create index before dropped column
drop table if exists bm.het_bm;
create table bm.het_bm (i int, to_be_dropped char(5), j int, t text) distributed by (i) partition by list(j)
  (partition part0 values(0) with (appendonly=true, compresslevel=5, orientation=column),
   partition part1 values(1),
   partition part2 values(2) with (appendonly=true, compresslevel=5, orientation=row),
   partition part3 values(3), partition part4 values(4));
insert into bm.het_bm select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;

-- Create index before dropping a column
create index het_bm_i_idx on bm.het_bm using bitmap (i);
create index het_bm_j_idx on bm.het_bm using bitmap (j);

alter table bm.het_bm drop column to_be_dropped;
alter table bm.het_bm add partition part5 values(5);
insert into bm.het_bm values(2, 5, '2');

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.het_bm where i=2 and j=2;
select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.het_bm where i=2 and t='2';

-- Drop and recreate index after we dropped column
drop index bm.het_bm_i_idx;
drop index bm.het_bm_j_idx;
create index het_bm_i_idx on bm.het_bm using bitmap (i);
create index het_bm_j_idx on bm.het_bm using bitmap (j);

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.het_bm where j=2 or j=3;

-- Create index after dropped column
drop table if exists bm.het_bm;
create table bm.het_bm (i int, to_be_dropped char(5), j int, t text) distributed by (i) partition by list(j)
  (partition part0 values(0) with (appendonly=true, compresslevel=5, orientation=column),
   partition part1 values(1),
   partition part2 values(2) with (appendonly=true, compresslevel=5, orientation=row),
   partition part3 values(3), partition part4 values(4));
insert into bm.het_bm select i % 10, 'drop', i % 5, (i % 10)::text  from generate_series(1, 100) i;

alter table bm.het_bm drop column to_be_dropped;
alter table bm.het_bm add partition part5 values(5);

-- Create index after dropping a column but before we insert into newly created part
create index het_bm_i_idx on bm.het_bm using bitmap (i);
create index het_bm_j_idx on bm.het_bm using bitmap (j);
insert into bm.het_bm values(2, 5, '2');

select sum(i) i_sum, sum(j) j_sum, sum(t::integer) t_sum from bm.het_bm where j=2 or j=3;

-- Rescan bitmap index
drop table if exists bm.outer_tab;
create table bm.outer_tab
(
id serial,
code character varying(25),
name character varying(40)
);

insert into bm.outer_tab select i % 10, i % 5, i % 2 from generate_series(1, 100) i;

set optimizer_enable_hashjoin = off;
select count(1) from bm.outer_tab;
select count(1) from bm.het_bm;

select sum(id) id_sum, sum(i) i_sum, sum(j) j_sum from bm.outer_tab as ot join bm.het_bm as het_bm on ot.id = het_bm.j
where het_bm.i between 2 and 5;

drop schema bm cascade;
reset optimizer_enable_hashjoin;
reset optimizer_enable_bitmapscan;
-- End of BitmapTableScan & BitmapIndexScan tests

set optimizer_enable_constant_expression_evaluation=on;

CREATE TABLE my_tt_agg_opt (
    symbol character(16),
    event_ts bigint,
    trade_price numeric,
    trade_volume bigint
) DISTRIBUTED BY (symbol);

CREATE TABLE my_tq_agg_opt_part (
    ets bigint,
    sym character varying(16),
    bid_price numeric,
    ask_price numeric,
    end_ts bigint
) DISTRIBUTED BY (ets)
   PARTITION BY RANGE(bid_price)(PARTITION p1 START(0) END(200000),
                                 PARTITION p2 START(200000) END(900000));

CREATE INDEX my_tq_agg_opt_part_ets_end_ts_ix_1 ON my_tq_agg_opt_part_1_prt_p1 USING btree (ets, end_ts);
CREATE INDEX my_tq_agg_opt_part_ets_end_ts_ix_2 ON my_tq_agg_opt_part_1_prt_p2 USING btree (ets);

CREATE FUNCTION plusone(numeric) RETURNS numeric AS $$
BEGIN
    SELECT $1 + 1;
END;
$$ LANGUAGE plpgsql volatile;

-- start_ignore
select disable_xform('CXformInnerJoin2DynamicIndexGetApply');
select disable_xform('CXformInnerJoin2HashJoin');
select disable_xform('CXformInnerJoin2IndexGetApply');
select disable_xform('CXformInnerJoin2NLJoin');
-- end_ignore

set optimizer_enable_partial_index=on;
set optimizer_enable_indexjoin=on;

-- force_explain
set optimizer_segments = 3;
EXPLAIN
SELECT (tt.event_ts / 100000) / 5 * 5 as fivemin, COUNT(*)
FROM my_tt_agg_opt tt, my_tq_agg_opt_part tq
WHERE tq.sym = tt.symbol AND
      plusone(tq.bid_price) < tt.trade_price AND
      tt.event_ts >= tq.ets AND
      tt.event_ts <  tq.end_ts
GROUP BY 1
ORDER BY 1 asc ;
reset optimizer_segments;
reset optimizer_enable_constant_expression_evaluation;
reset optimizer_enable_indexjoin;
reset optimizer_enable_partial_index;

-- start_ignore
select enable_xform('CXformInnerJoin2DynamicIndexGetApply');
select enable_xform('CXformInnerJoin2HashJoin');
select enable_xform('CXformInnerJoin2IndexGetApply');
select enable_xform('CXformInnerJoin2NLJoin');
-- end_ignore

-- MPP-25661: IndexScan crashing for qual with reference to outer tuple
drop table if exists idxscan_outer;
create table idxscan_outer
(
id serial,
code character varying(25),
name character varying(40)
);

drop table if exists idxscan_inner;
create table idxscan_inner
(
ordernum int unique,
productid int,
comment character varying(40)
);

-- Have more rows in one table, so that the planner will
-- choose to make that the outer side of the join.
insert into idxscan_outer values (1, 'a', 'aaa');
insert into idxscan_outer values (2, 'b', 'bbb');
insert into idxscan_outer values (3, 'c', 'ccc');
insert into idxscan_outer values (4, 'd', 'ddd');
insert into idxscan_outer values (5, 'e', 'eee');
insert into idxscan_outer values (6, 'f', 'fff');
insert into idxscan_outer values (7, 'g', 'ggg');
insert into idxscan_outer values (8, 'h', 'hhh');
insert into idxscan_outer values (9, 'i', 'iii');

insert into idxscan_inner values (11, 1, 'xxxx');
insert into idxscan_inner values (24, 2, 'yyyy');
insert into idxscan_inner values (13, 3, 'zzzz');

analyze idxscan_outer;
analyze idxscan_inner;

set optimizer_enable_hashjoin = off;

explain select id, comment from idxscan_outer as o join idxscan_inner as i on o.id = i.productid
where ordernum between 10 and 20;

select id, comment from idxscan_outer as o join idxscan_inner as i on o.id = i.productid
where ordernum between 10 and 20;

reset optimizer_enable_hashjoin;

drop table idxscan_outer;
drop table idxscan_inner;

drop table if exists ggg;
set optimizer_metadata_caching=on;

create table ggg (a char(1), b char(2), d char(3));
insert into ggg values ('x', 'a', 'c');
insert into ggg values ('x', 'a');
insert into ggg values ('x');

-- MPP-25700 Fix to TO_DATE on hybrid datums during constant expression evaluation
drop table if exists orca.t3;
create table orca.t3 (c1 timestamp without time zone);
insert into orca.t3 values  ('2015-07-03 00:00:00'::timestamp without time zone);
select to_char(c1, 'YYYY-MM-DD HH24:MI:SS') from orca.t3 where c1 = TO_DATE('2015-07-03','YYYY-MM-DD');
select to_char(c1, 'YYYY-MM-DD HH24:MI:SS') from orca.t3 where c1 = '2015-07-03'::date;

-- MPP-25806: multi-column index
create table orca.index_test (a int, b int, c int, d int, e int, constraint index_test_pkey PRIMARY KEY (a, b, c, d));
insert into orca.index_test select i,i%2,i%3,i%4,i%5 from generate_series(1,100) i;
-- force_explain
explain select * from orca.index_test where a = 5;

-- force_explain
explain select * from orca.index_test where c = 5;

-- force_explain
explain select * from orca.index_test where a = 5 and c = 5;

-- renaming columns
select * from (values (2),(null)) v(k);

-- Checking if ORCA correctly populates canSetTag in PlannedStmt for multiple statements because of rules
drop table if exists can_set_tag_target;
create table can_set_tag_target
(
	x int,
	y int,
	z char
);

drop table if exists can_set_tag_audit;
create table can_set_tag_audit
(
	t timestamp without time zone,
	x int,
	y int,
	z char
);

create rule can_set_tag_audit_update AS
    ON UPDATE TO can_set_tag_target DO  INSERT INTO can_set_tag_audit (t, x, y, z)
  VALUES (now(), old.x, old.y, old.z);

insert into can_set_tag_target select i, i + 1, i + 2 from generate_series(1,2) as i;

create role unpriv;
grant all on can_set_tag_target to unpriv;
grant all on can_set_tag_audit to unpriv;
set role unpriv;
show optimizer;
update can_set_tag_target set y = y + 1;
select count(1) from can_set_tag_audit;
reset role;

revoke all on can_set_tag_target from unpriv;
revoke all on can_set_tag_audit from unpriv;
drop role unpriv;
drop table can_set_tag_target;
drop table can_set_tag_audit;

-- start_ignore
create language plpythonu;
-- end_ignore

-- Checking if ORCA uses parser's canSetTag for CREATE TABLE AS SELECT
create or replace function canSetTag_Func(x int) returns int as $$
    if (x is None):
        return 0
    else:
        return x * 3
$$ language plpythonu;

create table canSetTag_input_data (domain integer, class integer, attr text, value integer)
   distributed by (domain);
insert into canSetTag_input_data values(1, 1, 'A', 1);
insert into canSetTag_input_data values(2, 1, 'A', 0);
insert into canSetTag_input_data values(3, 0, 'B', 1);

create table canSetTag_bug_table as
SELECT attr, class, (select canSetTag_Func(count(distinct class)::int) from canSetTag_input_data)
   as dclass FROM canSetTag_input_data GROUP BY attr, class distributed by (attr);

drop function canSetTag_Func(x int);
drop table canSetTag_bug_table;
drop table canSetTag_input_data;

-- Test B-Tree index scan with in list
CREATE TABLE btree_test as SELECT i a, i b FROM generate_series(1,100) i distributed randomly;
CREATE INDEX btree_test_index ON btree_test(a);
set optimizer_enable_tablescan = off;
-- start_ignore
select disable_xform('CXformSelect2IndexGet');
-- end_ignore
EXPLAIN SELECT * FROM btree_test WHERE a in (1, 47);
EXPLAIN SELECT * FROM btree_test WHERE a in ('2', 47);
EXPLAIN SELECT * FROM btree_test WHERE a in ('1', '2');
EXPLAIN SELECT * FROM btree_test WHERE a in ('1', '2', 47);
SELECT * FROM btree_test WHERE a in ('1', '2', 47);
CREATE INDEX btree_test_index_ab ON btree_test using btree(a,b);
EXPLAIN SELECT * FROM btree_test WHERE a in (1, 2, 47) AND b > 1;
SELECT * FROM btree_test WHERE a in (1, 2, 47) AND b > 1;
-- start_ignore
select enable_xform('CXformSelect2IndexGet');
-- end_ignore
reset optimizer_enable_tablescan;

-- Test Bitmap index scan with in list
CREATE TABLE bitmap_test as SELECT * FROM generate_series(1,100) as a distributed randomly;
CREATE INDEX bitmap_index ON bitmap_test USING BITMAP(a);
EXPLAIN SELECT * FROM bitmap_test WHERE a in (1);
EXPLAIN SELECT * FROM bitmap_test WHERE a in (1, 47);
EXPLAIN SELECT * FROM bitmap_test WHERE a in ('2', 47);
EXPLAIN SELECT * FROM bitmap_test WHERE a in ('1', '2');
EXPLAIN SELECT * FROM bitmap_test WHERE a in ('1', '2', 47);

-- Test Logging for unsupported features in ORCA
-- start_ignore
drop table if exists foo;
-- end_ignore

create table foo(a int, b int) distributed by (a);

-- The amount of log messages you get depends on a lot of options, but any
-- difference in the output will make the test fail. Disable log_statement
-- and log_min_duration_statement, they are the most obvious ones.
set log_statement='none';
set log_min_duration_statement=-1;
set client_min_messages='log';
explain select count(*) from foo group by cube(a,b);
reset client_min_messages;
reset log_statement;
reset log_min_duration_statement;

-- TVF accepts ANYENUM, ANYELEMENT returns ANYENUM, ANYARRAY
CREATE TYPE rainbow AS ENUM('red','yellow','blue');
CREATE FUNCTION func_enum_element(ANYENUM, ANYELEMENT) RETURNS TABLE(a ANYENUM, b ANYARRAY)
AS $$ SELECT $1, ARRAY[$2] $$ LANGUAGE SQL STABLE;
SELECT * FROM func_enum_element('red'::rainbow, 'blue'::rainbow::anyelement);
DROP FUNCTION IF EXISTS func_enum_element(ANYENUM, ANYELEMENT);

-- TVF accepts ANYELEMENT, ANYARRAY returns ANYELEMENT, ANYENUM
CREATE FUNCTION func_element_array(ANYELEMENT, ANYARRAY) RETURNS TABLE(a ANYELEMENT, b ANYENUM)
AS $$ SELECT $1, $2[1]$$ LANGUAGE SQL STABLE;
SELECT * FROM func_element_array('red'::rainbow, ARRAY['blue'::rainbow]);
DROP FUNCTION IF EXISTS func_element_array(ANYELEMENT, ANYARRAY);

-- TVF accepts ANYARRAY, ANYENUM returns ANYELEMENT
CREATE FUNCTION func_element_array(ANYARRAY, ANYENUM) RETURNS TABLE(a ANYELEMENT, b ANYELEMENT)
AS $$ SELECT $1[1], $2 $$ LANGUAGE SQL STABLE;
SELECT * FROM func_element_array(ARRAY['blue'::rainbow], 'blue'::rainbow);
DROP FUNCTION IF EXISTS func_element_array(ANYARRAY, ANYENUM);

-- TVF accepts ANYARRAY argument returns ANYELEMENT, ANYENUM
CREATE FUNCTION func_array(ANYARRAY) RETURNS TABLE(a ANYELEMENT, b ANYENUM)
AS $$ SELECT $1[1], $1[2] $$ LANGUAGE SQL STABLE;
SELECT * FROM func_array(ARRAY['blue'::rainbow, 'yellow'::rainbow]);
DROP FUNCTION IF EXISTS func_array(ANYARRAY);

-- TVF accepts ANYELEMENT, VARIADIC ARRAY returns ANYARRAY
CREATE FUNCTION func_element_variadic(ANYELEMENT, VARIADIC ANYARRAY) RETURNS TABLE(a ANYARRAY)
AS $$ SELECT array_prepend($1, $2); $$ LANGUAGE SQL STABLE;
SELECT * FROM func_element_variadic(1.1, 1.1, 2.2, 3.3);
DROP FUNCTION IF EXISTS func_element_variadic(ANYELEMENT, VARIADIC ANYARRAY);

-- TVF accepts ANYNONARRAY returns ANYNONARRAY
CREATE FUNCTION func_nonarray(ANYNONARRAY) RETURNS TABLE(a ANYNONARRAY)
AS $$ SELECT $1; $$ LANGUAGE SQL STABLE;
SELECT * FROM func_nonarray(5);
DROP FUNCTION IF EXISTS func_nonarray(ANYNONARRAY);

-- TVF accepts ANYNONARRAY, ANYENUM returns ANYARRAY
CREATE FUNCTION func_nonarray_enum(ANYNONARRAY, ANYENUM) RETURNS TABLE(a ANYARRAY)
AS $$ SELECT ARRAY[$1, $2]; $$ LANGUAGE SQL STABLE;
SELECT * FROM func_nonarray_enum('blue'::rainbow, 'red'::rainbow);
DROP FUNCTION IF EXISTS func_nonarray_enum(ANYNONARRAY, ANYENUM);

-- TVF accepts ANYARRAY, ANYNONARRAY, ANYENUM returns ANYNONARRAY
CREATE FUNCTION func_array_nonarray_enum(ANYARRAY, ANYNONARRAY, ANYENUM) RETURNS TABLE(a ANYNONARRAY)
AS $$ SELECT $1[1]; $$ LANGUAGE SQL STABLE;
SELECT * FROM func_array_nonarray_enum(ARRAY['blue'::rainbow, 'red'::rainbow], 'red'::rainbow, 'yellow'::rainbow);
DROP FUNCTION IF EXISTS func_array_nonarray_enum(ANYARRAY, ANYNONARRAY, ANYENUM);

-- start_ignore
drop table foo;
-- end_ignore

-- Test GPDB Expr (T_ArrayCoerceExpr) conversion to Scalar Array Coerce Expr
-- start_ignore
create table foo (a int, b character varying(10));
-- end_ignore
-- Query should not fallback to planner
explain select * from foo where b in ('1', '2');

set optimizer_enable_ctas = off;
set log_statement='none';
set log_min_duration_statement=-1;
set client_min_messages='log';
create table foo_ctas(a) as (select generate_series(1,10)) distributed by (a);
reset client_min_messages;
reset log_min_duration_statement;
reset log_statement;
reset optimizer_enable_ctas;

-- Test to ensure that ORCA produces correct results for a query with an Agg on top of LOJ
-- start_ignore
create table input_tab1 (a int, b int);
create table input_tab2 (c int, d int);
insert into input_tab1 values (1, 1);
insert into input_tab1 values (NULL, NULL);
set optimizer_force_multistage_agg = off;
set optimizer_force_three_stage_scalar_dqa = off;
-- end_ignore
explain (costs off) select count(*), t2.c from input_tab1 t1 left join input_tab2 t2 on t1.a = t2.c group by t2.c;
select count(*), t2.c from input_tab1 t1 left join input_tab2 t2 on t1.a = t2.c group by t2.c;

-- start_ignore
reset optimizer_force_multistage_agg;
reset optimizer_force_three_stage_scalar_dqa;
-- end_ignore

--
-- Test to ensure orca produces correct equivalence class for an alias projected by a LOJ and thus producing correct results.
-- Previously, orca produced an incorrect filter (cd2 = cd) on top of LOJ which led to incorrect results as column 'cd' is
-- produced by a nullable side of LOJ (tab2).
--
-- start_ignore
CREATE TABLE tab_1 (id VARCHAR(32)) DISTRIBUTED RANDOMLY;
INSERT INTO tab_1 VALUES('qwert'), ('vbn');

CREATE TABLE tab_2(key VARCHAR(200) NOT NULL, id VARCHAR(32) NOT NULL, cd VARCHAR(2) NOT NULL) DISTRIBUTED BY(key);
INSERT INTO tab_2 VALUES('abc', 'rew', 'dr');
INSERT INTO tab_2 VALUES('tyu', 'rer', 'fd');

CREATE TABLE tab_3 (region TEXT, code TEXT) DISTRIBUTED RANDOMLY;
INSERT INTO tab_3 VALUES('cvb' ,'tyu');
INSERT INTO tab_3 VALUES('hjj' ,'xyz');
-- end_ignore

EXPLAIN SELECT Count(*)
FROM   (SELECT *
        FROM   (SELECT tab_2.cd AS CD1,
                       tab_2.cd AS CD2
                FROM   tab_1
                       LEFT JOIN tab_2
                              ON tab_1.id = tab_2.id) f
        UNION ALL
        SELECT region,
               code
        FROM   tab_3)a;

SELECT Count(*)
FROM   (SELECT *
        FROM   (SELECT tab_2.cd AS CD1,
                       tab_2.cd AS CD2
                FROM   tab_1
                       LEFT JOIN tab_2
                              ON tab_1.id = tab_2.id) f
        UNION ALL
        SELECT region,
               code
        FROM   tab_3)a;

--
-- Test to ensure that ORCA produces correct results with both blocking and
-- streaming materialze as controlled by optimizer_enable_streaming_material
-- GUC.
--
-- start_ignore
create table t_outer (c1 integer);
create table t_inner (c2 integer);
insert into t_outer values (generate_series (1,10));
insert into t_inner values (generate_series (1,300));
-- end_ignore

set optimizer_enable_streaming_material = on;
select c1 from t_outer where not c1 =all (select c2 from t_inner);
set optimizer_enable_streaming_material = off;
select c1 from t_outer where not c1 =all (select c2 from t_inner);
reset optimizer_enable_streaming_material;

-- Ensure that ORCA rescans the subquery in case of skip-level correlation with
-- materialization
drop table if exists wst0, wst1, wst2;

create table wst0(a0 int, b0 int);
create table wst1(a1 int, b1 int);
create table wst2(a2 int, b2 int);

insert into wst0 select i, i from generate_series(1,10) i;
insert into wst1 select i, i from generate_series(1,10) i;
insert into wst2 select i, i from generate_series(1,10) i;

-- NB: the rank() is need to force materialization (via Sort) in the subplan
select count(*) from wst0 where exists (select 1, rank() over (order by wst1.a1) from wst1 where a1 = (select b2 from wst2 where a0=a2+5));


--
-- Test to ensure sane behavior when DML queries are optimized by ORCA by
-- enforcing a non-master gather motion, controlled by
-- optimizer_enable_gather_on_segment_for_DML GUC
--

--
-- CTAS with global-local aggregation
--
-- start_ignore
create table test1 (a int, b int);
insert into test1 select generate_series(1,100),generate_series(1,100);
-- end_ignore
create table t_new as select avg(a) from test1 join (select i from unnest(array[1,2,3]) i) t on (test1.a = t.i);
select * from t_new;

-- start_ignore
drop table t_new;
set optimizer_enable_gather_on_segment_for_DML=off;
-- end_ignore
create table t_new as select avg(a) from test1 join (select i from unnest(array[1,2,3]) i) t on (test1.a = t.i);
select * from t_new;

-- start_ignore
reset optimizer_enable_gather_on_segment_for_DML;
-- end_ignore

--
-- Insert with outer references in the subquery
--
-- start_ignore
create table x_tab(a int);
create table y_tab(a int);
create table z_tab(a int);

insert into x_tab values(1);
insert into y_tab values(0);
insert into z_tab values(1);
-- end_ignore

insert into x_tab select * from x_tab where exists (select * from x_tab where x_tab.a = (select x_tab.a + y_tab.a from y_tab));
select * from x_tab;

--
-- Insert with Union All with an universal child
--
insert into y_tab select 1 union all select a from x_tab limit 10;
select * from y_tab;

--
-- Insert with a function containing a SQL
--
create or replace function test_func_pg_stats()
returns integer
as $$ declare cnt int; begin execute 'select count(*) from pg_statistic' into cnt; return cnt; end $$
language plpgsql volatile READS SQL DATA;

insert into y_tab select test_func_pg_stats() from x_tab limit 2;
select count(*) from y_tab;

--
-- Delete with Hash Join with a universal child
--
delete from x_tab where exists (select z_tab.a from z_tab join (select 1 as g) as tab on z_tab.a = tab.g);
select * from x_tab;

-- start_ignore
drop table bar;
-- end_ignore
-- TVF with a subplan that generates an RTABLE entry
create table bar(name text);
insert into bar values('person');
select * from unnest((select string_to_array(name, ',') from bar)) as a;

-- Query should not fall back to planner and handle implicit cast properly

CREATE TYPE myint;

CREATE FUNCTION myintout(myint) RETURNS cstring AS 'int4out' LANGUAGE INTERNAL STRICT IMMUTABLE;
CREATE FUNCTION myintin(cstring) RETURNS myint AS 'int4in' LANGUAGE INTERNAL STRICT IMMUTABLE;

CREATE TYPE myint
(
	INPUT=myintin,
	OUTPUT=myintout,
	INTERNALLENGTH=4,
	PASSEDBYVALUE
);

CREATE FUNCTION myint_int8(myint) RETURNS int8 AS 'int48' LANGUAGE INTERNAL STRICT IMMUTABLE;

CREATE CAST (myint AS int8) WITH FUNCTION myint_int8(myint) AS IMPLICIT;

CREATE TABLE csq_cast_param_outer (a int, b myint);
INSERT INTO csq_cast_param_outer VALUES
	(1, '42'),
	(2, '12');

CREATE TABLE csq_cast_param_inner (c int, d myint);
INSERT INTO csq_cast_param_inner VALUES
	(11, '11'),
	(101, '12');

EXPLAIN SELECT a FROM csq_cast_param_outer WHERE b in (SELECT CASE WHEN a > 1 THEN d ELSE '42' END FROM csq_cast_param_inner);
SELECT a FROM csq_cast_param_outer WHERE b in (SELECT CASE WHEN a > 1 THEN d ELSE '42' END FROM csq_cast_param_inner);

DROP CAST (myint as int8);

CREATE FUNCTION myint_numeric(myint) RETURNS numeric AS 'int4_numeric' LANGUAGE INTERNAL STRICT IMMUTABLE;
CREATE CAST (myint AS numeric) WITH FUNCTION myint_numeric(myint) AS IMPLICIT;

EXPLAIN SELECT a FROM csq_cast_param_outer WHERE b in (SELECT CASE WHEN a > 1 THEN d ELSE '42' END FROM csq_cast_param_inner);
SELECT a FROM csq_cast_param_outer WHERE b in (SELECT CASE WHEN a > 1 THEN d ELSE '42' END FROM csq_cast_param_inner);

SELECT a FROM ggg WHERE a IN (NULL, 'x');

EXPLAIN SELECT a FROM ggg WHERE a NOT IN (NULL, '');

EXPLAIN SELECT a FROM ggg WHERE a IN (NULL, 'x');


-- result node with one time filter and filter
CREATE TABLE onetimefilter1 (a int, b int);
CREATE TABLE onetimefilter2 (a int, b int);
INSERT INTO onetimefilter1 SELECT i, i FROM generate_series(1,10)i;
INSERT INTO onetimefilter2 SELECT i, i FROM generate_series(1,10)i;
ANALYZE onetimefilter1;
ANALYZE onetimefilter2;
EXPLAIN WITH abc AS (SELECT onetimefilter1.a, onetimefilter1.b FROM onetimefilter1, onetimefilter2 WHERE onetimefilter1.a=onetimefilter2.a) SELECT (SELECT 1 FROM abc WHERE f1.b = f2.b LIMIT 1), COALESCE((SELECT 2 FROM abc WHERE f1.a=random() AND f1.a=2), 0), (SELECT b FROM abc WHERE b=f1.b) FROM onetimefilter1 f1, onetimefilter2 f2 WHERE f1.b = f2.b;
WITH abc AS (SELECT onetimefilter1.a, onetimefilter1.b FROM onetimefilter1, onetimefilter2 WHERE onetimefilter1.a=onetimefilter2.a) SELECT (SELECT 1 FROM abc WHERE f1.b = f2.b LIMIT 1), COALESCE((SELECT 2 FROM abc WHERE f1.a=random() AND f1.a=2), 0), (SELECT b FROM abc WHERE b=f1.b) FROM onetimefilter1 f1, onetimefilter2 f2 WHERE f1.b = f2.b;


-- full joins with predicates
DROP TABLE IF EXISTS ffoo, fbar;
CREATE TABLE ffoo (a, b) AS (VALUES (1, 2), (2, 3), (4, 5), (5, 6), (6, 7)) DISTRIBUTED BY (a);
CREATE TABLE fbar (c, d) AS (VALUES (1, 42), (2, 43), (4, 45), (5, 46)) DISTRIBUTED BY (c);

SELECT d FROM ffoo FULL OUTER JOIN fbar ON a = c WHERE b BETWEEN 5 and 9;

-- test index left outer joins on bitmap and btree indexes on partitioned tables with and without select clause
DROP TABLE IF EXISTS touter, tinner;
CREATE TABLE touter(a int, b int) DISTRIBUTED BY (a);
CREATE TABLE tinnerbitmap(a int, b int) DISTRIBUTED BY (a) PARTITION BY range(b) (start (0) end (6) every (3));
CREATE TABLE tinnerbtree(a int, b int) DISTRIBUTED BY (a) PARTITION BY range(b) (start (0) end (6) every (3));

INSERT INTO touter SELECT i, i%6 FROM generate_series(1,10) i;
INSERT INTO tinnerbitmap select i, i%6 FROM generate_series(1,1000) i;
INSERT INTO tinnerbtree select i, i%6 FROM generate_series(1,1000) i;
CREATE INDEX tinnerbitmap_ix ON tinnerbitmap USING bitmap(a);
CREATE INDEX tinnerbtree_ix ON tinnerbtree USING btree(a);

SELECT * FROM touter LEFT JOIN tinnerbitmap ON touter.a = tinnerbitmap.a;
SELECT * FROM touter LEFT JOIN tinnerbitmap ON touter.a = tinnerbitmap.a AND tinnerbitmap.b=10;

SELECT * FROM touter LEFT JOIN tinnerbtree ON touter.a = tinnerbtree.a;
SELECT * FROM touter LEFT JOIN tinnerbtree ON touter.a = tinnerbtree.a AND tinnerbtree.b=10;

-- test subplan in a qual under dynamic scan
CREATE TABLE ds_part ( a INT, b INT, c INT) PARTITION BY RANGE(c)( START(1) END (10) EVERY (2), DEFAULT PARTITION deflt);
CREATE TABLE non_part1 (c INT);
CREATE TABLE non_part2 (e INT, f INT);

INSERT INTO ds_part SELECT i, i, i FROM generate_series (1, 1000)i; 
INSERT INTO non_part1 SELECT i FROM generate_series(1, 100)i; 
INSERT INTO non_part2 SELECT i, i FROM generate_series(1, 100)i;

SET optimizer_enforce_subplans TO ON;
analyze ds_part;
analyze non_part1;
analyze non_part2;
SELECT * FROM ds_part, non_part2 WHERE ds_part.c = non_part2.e AND non_part2.f = 10 AND a IN ( SELECT b + 1 FROM non_part1);
explain SELECT * FROM ds_part, non_part2 WHERE ds_part.c = non_part2.e AND non_part2.f = 10 AND a IN ( SELECT b + 1 FROM non_part1);

SELECT *, a IN ( SELECT b + 1 FROM non_part1) FROM ds_part, non_part2 WHERE ds_part.c = non_part2.e AND non_part2.f = 10 AND a IN ( SELECT b FROM non_part1);
CREATE INDEX ds_idx ON ds_part(a);
analyze ds_part;
SELECT *, a IN ( SELECT b + 1 FROM non_part1) FROM ds_part, non_part2 WHERE ds_part.c = non_part2.e AND non_part2.f = 10 AND a IN ( SELECT b FROM non_part1);

RESET optimizer_enforce_subplans;
-- implied predicate must be generated for the type cast(ident) scalar array cmp const array
CREATE TABLE varchar_sc_array_cmp(a varchar);
INSERT INTO varchar_sc_array_cmp VALUES ('a'), ('b'), ('c'), ('d');
EXPLAIN SELECT * FROM varchar_sc_array_cmp t1, varchar_sc_array_cmp t2 where t1.a = t2.a and t1.a in ('b', 'c');
SELECT * FROM varchar_sc_array_cmp t1, varchar_sc_array_cmp t2 where t1.a = t2.a and t1.a in ('b', 'c');
SET optimizer_array_constraints=on;
EXPLAIN SELECT * FROM varchar_sc_array_cmp t1, varchar_sc_array_cmp t2 where t1.a = t2.a and (t1.a in ('b', 'c') OR t1.a = 'a');
SELECT * FROM varchar_sc_array_cmp t1, varchar_sc_array_cmp t2 where t1.a = t2.a and (t1.a in ('b', 'c') OR t1.a = 'a');
DROP TABLE varchar_sc_array_cmp;

-- table constraints on nullable columns
-- start_ignore
DROP TABLE IF EXISTS tc0, tc1, tc2, tc3, tc4;
-- end_ignore
CREATE TABLE tc0 (a int check (a = 5));
INSERT INTO tc0 VALUES (NULL);
-- FIXME: Planner gives wrong result
SELECT * from tc0 where a IS NULL;

CREATE TABLE tc1 (a int check (a between 1 and 2 or a != 3 and a > 5));
INSERT INTO tc1 VALUES (NULL);
SELECT * from tc1 where a IS NULL;

CREATE TABLE tc2 (a int check (a in (1,2)));
INSERT INTO tc2 VALUES (NULL);
SELECT * from tc2 where a IS NULL;

set optimizer_array_constraints = on;
CREATE TABLE tc3 (a int check (a = ANY (ARRAY[1,2])));
INSERT INTO tc3 VALUES (NULL);
SELECT * from tc3 where a IS NULL;
reset optimizer_array_constraints;

CREATE TABLE tc4 (a int, b int, check(a + b > 1 and a = b));
INSERT INTO tc4 VALUES(NULL, NULL);
SELECT * from tc4 where a IS NULL;

CREATE EXTENSION IF NOT EXISTS citext;
drop table if exists tt, tc;
create table tc (a int, c citext) distributed by (a);
create table tt (b int, v varchar) distributed by (v);

insert into tc values (1, 'a'), (1, 'A');
insert into tt values (1, 'a'), (1, 'A');

insert into tc values (1, 'b'), (1, 'B');
insert into tt values (1, 'b'), (1, 'B');

select * from tc, tt where c = v;

-- test gpexpand phase 1
-- right now, these will fall back to planner

drop table if exists noexp_hash, gpexp_hash, gpexp_rand, gpexp_repl;
create table noexp_hash(a int, b int) distributed by (a);
insert into  noexp_hash select i, i from generate_series(1,50) i;

-- three tables that will be expanded (simulated)
create table gpexp_hash(a int, b int) distributed by (a);
create table gpexp_rand(a int, b int) distributed randomly;
create table gpexp_repl(a int, b int) distributed replicated;

-- simulate a cluster with one segment less than we have now
set allow_system_table_mods = true;
update gp_distribution_policy set numsegments = numsegments-1 where localoid = 'gpexp_hash'::regclass and numsegments > 1;
update gp_distribution_policy set numsegments = numsegments-1 where localoid = 'gpexp_rand'::regclass and numsegments > 1;
update gp_distribution_policy set numsegments = numsegments-1 where localoid = 'gpexp_repl'::regclass and numsegments > 1;
reset allow_system_table_mods;

-- populate the tables on this smaller cluster
explain insert into gpexp_hash select i, i from generate_series(1,50) i;

insert into gpexp_hash select i, i from generate_series(1,50) i;
insert into gpexp_rand select i, i from generate_series(1,50) i;
insert into gpexp_repl select i, i from generate_series(1,50) i;

-- the segment ids in the unmodified table should have one extra number
select max(noexp_hash.gp_segment_id) - max(gpexp_hash.gp_segment_id) as expect_one
from noexp_hash, gpexp_hash;

-- join should have a redistribute motion for gpexp_hash
explain select count(*) from noexp_hash n join gpexp_hash x on n.a=x.a;
select count(*) from noexp_hash n join gpexp_hash x on n.a=x.a;
delete from gpexp_hash where b between 21 and 50;
select count(*) from gpexp_hash;
update gpexp_hash set b=-1 where b between 11 and 100;
select b, count(*) from gpexp_hash group by b order by b;

explain update gpexp_rand set b=(select b from gpexp_hash where gpexp_rand.a = gpexp_hash.a);
update gpexp_rand set b=(select b from gpexp_hash where gpexp_rand.a = gpexp_hash.a);
select b, count(*) from gpexp_rand group by b order by b;

delete from gpexp_repl where b >= 20;
explain insert into gpexp_repl values (20, 20);
insert into gpexp_repl values (20, 20);

explain select count(*) from gpexp_hash h join gpexp_repl r on h.a=r.a;
select count(*) as expect_20 from gpexp_hash h join gpexp_repl r on h.a=r.a;
explain select count(*) as expect_20 from noexp_hash h join gpexp_repl r on h.a=r.a;
select count(*) as expect_20 from noexp_hash h join gpexp_repl r on h.a=r.a;

create table part1(a int, b int) partition by range(b) (start(1) end(5) every(1));
create table part2(a int, b int) partition by range(b) (start(1) end(5) every(1));
insert into part1 select i, (i % 2) + 1 from generate_series(1, 1000) i;
insert into part2 select i, (i % 2) + 1 from generate_series(1, 100) i;
-- make sure some child partitions have not been analyzed. This just means that
-- stats are missing for some child partition but not necessarily that the relation
-- is empty. So we should not flag this as an empty relation 
analyze part1_1_prt_1;
analyze part1_1_prt_2;
analyze part2_1_prt_1;
analyze part2_1_prt_2;
-- the plan should contain a 2 stage limit. If we incorrectly estimate that the
-- relation is empty, we would end up choosing a single stage limit. 
explain select * from part1, part2 where part1.b = part2.b limit 5;

-- test opfamily handling in ORCA
-- start_ignore
DROP FUNCTION abseq(int, int) CASCADE;
DROP FUNCTION abslt(int, int) CASCADE;
DROP FUNCTION absgt(int, int) CASCADE;
DROP FUNCTION abscmp(int, int) CASCADE;
-- end_ignore
CREATE FUNCTION abseq(int, int) RETURNS BOOL AS
$$
  begin return abs($1) = abs($2); end;
$$ LANGUAGE plpgsql STRICT IMMUTABLE;

CREATE OPERATOR |=| (
  PROCEDURE = abseq,
  LEFTARG = int,
  RIGHTARG = int,
  COMMUTATOR = |=|,
  hashes, merges);

CREATE FUNCTION abshashfunc(int) RETURNS int AS
$$
  begin return hashint4(abs($1)); end;
$$ LANGUAGE plpgsql STRICT IMMUTABLE;

CREATE FUNCTION abslt(int, int) RETURNS BOOL AS
$$
  begin return abs($1) < abs($2); end;
$$ LANGUAGE plpgsql STRICT IMMUTABLE;

CREATE OPERATOR |<| (
  PROCEDURE = abslt,
  LEFTARG = int,
  RIGHTARG = int);

CREATE FUNCTION absgt(int, int) RETURNS BOOL AS
$$
  begin return abs($1) > abs($2); end;
$$ LANGUAGE plpgsql STRICT IMMUTABLE;

CREATE OPERATOR |>| (
  PROCEDURE = absgt,
  LEFTARG = int,
  RIGHTARG = int);

CREATE FUNCTION abscmp(int, int) RETURNS int AS
$$
  begin return btint4cmp(abs($1),abs($2)); end;
$$ LANGUAGE plpgsql STRICT IMMUTABLE;

DROP TABLE IF EXISTS atab_old_hash;
DROP TABLE IF EXISTS btab_old_hash;
CREATE TABLE atab_old_hash (a int) DISTRIBUTED BY (a);
CREATE TABLE btab_old_hash (b int) DISTRIBUTED BY (b);

INSERT INTO atab_old_hash VALUES (-1), (0), (1);
INSERT INTO btab_old_hash VALUES (-1), (0), (1), (2);

-- Test simple join using the new operator(s) before creating the opclass/opfamily
EXPLAIN SELECT a, b FROM atab_old_hash INNER JOIN btab_old_hash ON a |=| b;
SELECT a, b FROM atab_old_hash INNER JOIN btab_old_hash ON a |=| b;

CREATE OPERATOR CLASS abs_int_hash_ops FOR TYPE int4
  USING hash AS
  OPERATOR 1 |=|,
  FUNCTION 1 abshashfunc(int);

CREATE OPERATOR CLASS abs_int_btree_ops FOR TYPE int4
  USING btree AS
  OPERATOR 1 |<|,
  OPERATOR 3 |=|,
  OPERATOR 5 |>|,
  FUNCTION 1 abscmp(int, int);

-- OK test different kinds of joins
EXPLAIN SELECT a, b FROM atab_old_hash INNER JOIN btab_old_hash ON a |=| b;
SELECT a, b FROM atab_old_hash INNER JOIN btab_old_hash ON a |=| b;

EXPLAIN SELECT a, b FROM btab_old_hash LEFT OUTER JOIN atab_old_hash ON a |=| b;
SELECT a, b FROM btab_old_hash LEFT OUTER JOIN atab_old_hash ON a |=| b;

set optimizer_expand_fulljoin = on;
EXPLAIN SELECT a, b FROM atab_old_hash FULL JOIN btab_old_hash ON a |=| b;
SELECT a, b FROM atab_old_hash FULL JOIN btab_old_hash ON a |=| b;
reset optimizer_expand_fulljoin;

-- Test rescanned materialize that is not directly above a motion

DROP TABLE IF EXISTS foo1 CASCADE;
DROP TABLE IF EXISTS foo2 CASCADE;
DROP TABLE IF EXISTS foo3 CASCADE;
CREATE table foo1(a int);
CREATE table foo2(a int, b int, c int);
CREATE table foo3(a int, b int);
CREATE index f2c on foo2 using bitmap(c);
INSERT INTO foo1 values (1), (2);
INSERT INTO foo2 values (1,1,1), (2,2,2);
INSERT INTO foo3 values (1,1), (2,2);

set optimizer_join_order=query;
select disable_xform('CXformInnerJoin2HashJoin');

EXPLAIN SELECT 1 FROM foo1, foo2 WHERE foo1.a = foo2.a AND foo2.c = 3 AND foo2.b IN (SELECT b FROM foo3);
SELECT 1 FROM foo1, foo2 WHERE foo1.a = foo2.a AND foo2.c = 3 AND foo2.b IN (SELECT b FROM foo3);

reset optimizer_join_order;
select enable_xform('CXformInnerJoin2HashJoin');

drop table if exists t55;
drop table if exists tp;

create table t55 (c int, lid int);
insert into t55 select i, i from generate_series(1, 1000) i;

set optimizer_join_order = query;

explain verbose
CREATE TABLE TP AS
WITH META AS (SELECT '2020-01-01' AS VALID_DT, '99' AS LOAD_ID)
SELECT DISTINCT L1.c, L1.lid
FROM t55 L1 CROSS JOIN META
WHERE L1.lid = int4in(unknownout(meta.load_id));

CREATE TABLE TP AS
WITH META AS (SELECT '2020-01-01' AS VALID_DT, '99' AS LOAD_ID)
SELECT DISTINCT L1.c, L1.lid
FROM t55 L1 CROSS JOIN META
WHERE L1.lid = int4in(unknownout(meta.load_id));

reset optimizer_join_order;
SELECT * from tp;

-- start_ignore
DROP SCHEMA orca CASCADE;
-- end_ignore
