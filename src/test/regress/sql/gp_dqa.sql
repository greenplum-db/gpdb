-- The last digits of some of the results vary from one invocation to another,
-- because the intermediate operations are done in arbitrary order when rows
-- are flowing from different segments in different order. Mask those
-- differences by setting 'extra_float_digits'. This isn't enough for all of
-- the queries, so a few also use TO_CHAR() to truncate the results further.
set extra_float_digits=0;

SET optimizer_trace_fallback to on;

drop table if exists dqa_t1;
drop table if exists dqa_t2;

create table dqa_t1 (d int, i int, c char, dt date);
create table dqa_t2 (d int, i int, c char, dt date);

insert into dqa_t1 select i%23, i%12, (i%10) || '', '2009-06-10'::date + ( (i%34) || ' days')::interval
from generate_series(0, 99) i;
insert into dqa_t2 select i%34, i%45, (i%10) || '', '2009-06-10'::date + ( (i%56) || ' days')::interval
from generate_series(0, 99) i;
analyze dqa_t1;
analyze dqa_t2;

-- With the default very small cost, the planner often prefer to just Gather
-- all the rows to the QD. We want to test the more complicated multi-tage DQA
-- plans here, without using a huge number of rows.
set gp_motion_cost_per_row=1;

set enable_hashagg=on;
set enable_groupagg=off;

-- Also run EXPLAIN on each of these queries, to make sure you get an efficient plan,
-- and not e.g. a naive one that just pulls all the rows to the QD.

-- Distinct keys are distribution keys
select count(distinct d) from dqa_t1;
explain (costs off) select count(distinct d) from dqa_t1;
select count(distinct d) from dqa_t1 group by i;
explain (costs off) select count(distinct d) from dqa_t1 group by i;

select count(distinct d), sum(distinct d) from dqa_t1 group by i;
explain (costs off) select count(distinct d), sum(distinct d) from dqa_t1 group by i;

select count(distinct d), count(distinct dt) from dqa_t1;
explain (costs off) select count(distinct d), count(distinct dt) from dqa_t1;
select count(distinct d), count(distinct c), count(distinct dt) from dqa_t1;
explain (costs off) select count(distinct d), count(distinct c), count(distinct dt) from dqa_t1;

select count(distinct d), count(distinct dt) from dqa_t1 group by c;
explain (costs off) select count(distinct d), count(distinct dt) from dqa_t1 group by c;
select count(distinct d), count(distinct dt) from dqa_t1 group by d;
explain (costs off) select count(distinct d), count(distinct dt) from dqa_t1 group by d;

select count(distinct dqa_t1.d) from dqa_t1, dqa_t2 where dqa_t1.d = dqa_t2.d;
explain (costs off) select count(distinct dqa_t1.d) from dqa_t1, dqa_t2 where dqa_t1.d = dqa_t2.d;
select count(distinct dqa_t1.d) from dqa_t1, dqa_t2 where dqa_t1.d = dqa_t2.d group by dqa_t2.dt;
explain (costs off) select count(distinct dqa_t1.d) from dqa_t1, dqa_t2 where dqa_t1.d = dqa_t2.d group by dqa_t2.dt;

-- Distinct keys are not distribution keys
select count(distinct c) from dqa_t1;
explain (costs off) select count(distinct c) from dqa_t1;
select count(distinct c) from dqa_t1 group by dt;
explain (costs off) select count(distinct c) from dqa_t1 group by dt;
select count(distinct c) from dqa_t1 group by d;
explain (costs off) select count(distinct c) from dqa_t1 group by d;

select count(distinct i), sum(distinct i) from dqa_t1 group by c;
explain (costs off) select count(distinct i), sum(distinct i) from dqa_t1 group by c;

select count(distinct c), count(distinct dt) from dqa_t1;
explain (costs off) select count(distinct c), count(distinct dt) from dqa_t1;
select count(distinct c), count(distinct dt), i from dqa_t1 group by i;
explain (costs off) select count(distinct c), count(distinct dt), i from dqa_t1 group by i;
select count(distinct i), count(distinct c), d from dqa_t1 group by d;
explain (costs off) select count(distinct i), count(distinct c), d from dqa_t1 group by d;

select count(distinct dqa_t1.dt) from dqa_t1, dqa_t2 where dqa_t1.c = dqa_t2.c;
explain (costs off) select count(distinct dqa_t1.dt) from dqa_t1, dqa_t2 where dqa_t1.c = dqa_t2.c;
select count(distinct dqa_t1.dt) from dqa_t1, dqa_t2 where dqa_t1.c = dqa_t2.c group by dqa_t2.dt;
explain (costs off) select count(distinct dqa_t1.dt) from dqa_t1, dqa_t2 where dqa_t1.c = dqa_t2.c group by dqa_t2.dt;

-- multidqa with groupby and order by
select sum(distinct d), count(distinct i), count(distinct c),i,c from dqa_t1 group by i,c order by i,c;
explain (costs off) select sum(distinct d), count(distinct i), count(distinct c),i,c from dqa_t1 group by i,c order by i,c;

-- multi args singledqa
select corr(distinct d, i) from dqa_t1;
explain (costs off) select corr(distinct d, i) from dqa_t1;

-- multi args singledqa with group by
select corr(distinct d, i) from dqa_t1 group by d;
explain (costs off) select corr(distinct d, i) from dqa_t1 group by d;

select to_char(corr(distinct d, i), '9.99999999999999') from dqa_t1 group by c;
explain (costs off) select to_char(corr(distinct d, i), '9.99999999999999') from dqa_t1 group by c;

-- multi args multidqa
select count(distinct c), corr(distinct d, i) from dqa_t1;
explain (costs off) select count(distinct c), corr(distinct d, i) from dqa_t1;

select count(distinct d), corr(distinct d, i) from dqa_t1;
explain (costs off) select count(distinct d), corr(distinct d, i) from dqa_t1;

select count(distinct d), count(distinct i), corr(distinct d, i) from dqa_t1;
explain (costs off) select count(distinct d), count(distinct i), corr(distinct d, i) from dqa_t1;

select count(distinct c), count(distinct d), count(distinct i), corr(distinct d, i) from dqa_t1;
explain (costs off) select count(distinct c), count(distinct d), count(distinct i), corr(distinct d, i) from dqa_t1;

-- multi args multidqa with group by
select count(distinct c), corr(distinct d, i), d from dqa_t1 group by d;
explain (costs off) select count(distinct c), corr(distinct d, i), d from dqa_t1 group by d;

select count(distinct c), corr(distinct d, i), d, i from dqa_t1 group by d,i;
explain (costs off) select count(distinct c), corr(distinct d, i), d, i from dqa_t1 group by d,i;

select count(distinct c), corr(distinct d, i), dt from dqa_t1 group by dt;
explain (costs off) select count(distinct c), corr(distinct d, i), dt from dqa_t1 group by dt;

select count(distinct d), corr(distinct d, i), i from dqa_t1 group by i;
explain (costs off) select count(distinct d), corr(distinct d, i), i from dqa_t1 group by i;

select count(distinct d), corr(distinct d, i), d from dqa_t1 group by d;
explain (costs off) select count(distinct d), corr(distinct d, i), d from dqa_t1 group by d;

select count(distinct d),  to_char(corr(distinct d, i), '9.99999999999999'), c from dqa_t1 group by c;
explain (costs off) select count(distinct d),  to_char(corr(distinct d, i), '9.99999999999999'), c from dqa_t1 group by c;

-- MPP-19037
drop table if exists fact_route_aggregation;
drop table if exists dim_devices;

CREATE TABLE fact_route_aggregation
(      
    device_id integer,
    is_route integer ,   
    is_pedestrian integer,
    user_id integer,
    pedestrian_route_length_in_meters integer,
    in_car_route_length_in_meters integer 
) DISTRIBUTED BY (device_id);

insert into fact_route_aggregation select generate_series(1,700),generate_series(200,300),generate_series(300,400), generate_series(400,500),generate_series(500,600),generate_series(600,700);

CREATE TABLE dim_devices
(      
    device_id integer,
    platform integer
) DISTRIBUTED BY (device_id);

-- Repro query from the JIRA
select  distinct 
count(distinct case  when T218094.is_route >= 1 or T218094.is_pedestrian >= 1 then T218094.user_id else NULL end ) as c1,
     sum(cast(T218094.is_route + T218094.is_pedestrian as  DOUBLE PRECISION  )) as c2,
     sum(cast(T218094.is_pedestrian as  DOUBLE PRECISION  )) as c3,
     count(distinct case  when T218094.is_pedestrian >= 1 then T218094.user_id else NULL end ) as c4,
     sum(T218094.pedestrian_route_length_in_meters / 1000.0) as c5,
     sum(T218094.in_car_route_length_in_meters / 1000.0) as c6,
     sum(cast(T218094.is_route as  DOUBLE PRECISION  )) as c7,
     count(distinct case  when T218094.is_route >= 1 then T218094.user_id else NULL end ) as c8,
     T43883.platform as c9
from 
     dim_devices T43883,
     fact_route_aggregation T218094
where  ( T43883.device_id = T218094.device_id ) 
group by T43883.platform;

-- cleanup
drop table fact_route_aggreagation;
drop table dim_devices;


-- other test queries for mpp-19037
drop table if exists t1_mdqa;
drop table if exists t2_mdqa;

create table t1_mdqa(a int, b int, c varchar);
create table t2_mdqa(a int, b int, c varchar);

insert into t1_mdqa select i % 5 , i % 10, i || 'value' from generate_series(1, 20) i;
insert into t1_mdqa select i % 5 , i % 10, i || 'value' from generate_series(1, 20) i;

insert into t2_mdqa select i % 10 , i % 5, i || 'value' from generate_series(1, 20) i;
insert into t2_mdqa select i % 10 , i % 5, i || 'value' from generate_series(1, 20) i;

-- simple mdqa
select count(distinct t1.a), count(distinct t2.b), t1.c, t2.c from t1_mdqa t1, t2_mdqa t2 where t1.c = t2.c group by t1.c, t2.c order by t1.c;

-- distinct on top of some mdqas
select distinct sum(distinct t1.a), avg(t2.a), sum(distinct t2.b), t1.a, t2.b from t1_mdqa t1, t2_mdqa t2 where t1.a = t2.a group by t1.a, t2.b order by t1.a;

select distinct sum (distinct t1.a), avg(distinct t2.a), sum(distinct t2.b), t1.c from t1_mdqa t1, t2_mdqa t2 where t1.a = t2.a group by t1.c order by t1.c;

-- distinct on group by fields
select distinct t1.c , sum(distinct t1.a), count(t2.b), sum(distinct t2.b) from t1_mdqa t1, t2_mdqa t2 where t1.a = t2.a group by t1.c order by t1.c;


-- distinct on normal aggregates
select distinct sum(t1.a), avg(distinct t2.a), sum(distinct (t1.a + t2.a)), t1.a, t2.b from t1_mdqa t1, t2_mdqa t2 where t1.a = t2.a group by t1.a, t2.b order by t1.a;

select distinct avg(t1.a + t2.b), count(distinct t1.c), count(distinct char_length(t1.c)), t1.a, t2.b from t1_mdqa t1, t2_mdqa t2 where t1.a = t2.a group by t1.a, t2.b order by t1.a;


-- cleanup
drop table t1_mdqa;
drop table t2_mdqa;


-- other queries from MPP-19037
drop table if exists gp_dqa_r;
drop table if exists gp_dqa_s;

create table gp_dqa_r (a int, b int, c int);
create table gp_dqa_s (d int, e int, f int);

insert into gp_dqa_r  select i , i %10, i%5 from generate_series(1,20) i;
insert into gp_dqa_s select i, i %15, i%10 from generate_series(1,30) i;

select a, d, count(distinct b) as c1, count(distinct c) as c2 from gp_dqa_r, gp_dqa_s where ( e = a ) group by d, a order by a,d;

select distinct 
count(distinct case when b >= 1 or c >= 1 then b else NULL end ) as c1,
sum(cast(b + c as DOUBLE PRECISION )) as c2,
sum(cast(c as DOUBLE PRECISION )) as c3,
count(distinct case when b >= 1 then b else NULL end ) as c2,
d as c9
from gp_dqa_r, gp_dqa_s
where ( e = a ) 
group by d order by c9;


select distinct 
count(distinct case when b >= 1 or c >= 1 then b else NULL end ) as c1,
count(distinct case when b >= 1 then b else NULL end ) as c2,
d as c9
from gp_dqa_r, gp_dqa_s
where ( e = a ) 
group by d order by c9;


select distinct count(distinct b) as c1, count(distinct c) as c2, d as c9
from gp_dqa_r, gp_dqa_s
where ( e = a ) 
group by d order by c9;

select distinct d, count(distinct b) as c1, count(distinct c) as c2, d as c9 from gp_dqa_r, gp_dqa_s group by d order by c9;

select distinct d, count(distinct b) as c1, count(distinct c) as c2, d as c9 from gp_dqa_r, gp_dqa_s group by d, a order by c9;

select distinct count(distinct b) as c1, count(distinct c) as c2 from gp_dqa_r, gp_dqa_s;

select distinct count(distinct b) as c1, count(distinct c) as c2 from gp_dqa_r;

select distinct count(distinct b) as c1, count(distinct c) as c2, d, a from gp_dqa_r, gp_dqa_s where ( e = a)group by d, a order by a,d;

select distinct count(distinct b) as c1, count(distinct c) as c2, d from gp_dqa_r, gp_dqa_s group by d, a order by d,a;

select distinct count(distinct b) as c1, count(distinct c) as c2, d from gp_dqa_r, gp_dqa_s group by d, a order by d;

select distinct count(distinct b) as c1, count(distinct c) as c2, d from gp_dqa_r, gp_dqa_s group by d order by d;

-- cleanup
drop table gp_dqa_r;
drop table gp_dqa_s;

-- setup
drop table if exists gp_dqa_t1;
drop table if exists gp_dqa_t2;
create table gp_dqa_t1 (a int, b int) distributed by (a);
create table gp_dqa_t2 (a int, c int) distributed by (a);

insert into gp_dqa_t1 select i , i %5 from generate_series(1,10) i;
insert into gp_dqa_t2 select i , i %4 from generate_series(1,10) i;

select distinct A.a, sum(distinct A.b), count(distinct B.c) from gp_dqa_t1 A left join gp_dqa_t2 B on (A.a = B.a) group by A.a order by A.a;

select distinct A.a, sum(distinct A.b), count(distinct B.c) from gp_dqa_t1 A right join gp_dqa_t2 B on (A.a = B.a) group by A.a order by A.a;

-- Most of the above test queries got planned as hash aggregates. Repeat
-- a few of them as group aggregates
set enable_hashagg=off;
set enable_groupagg=on;

select count(distinct d) from dqa_t1 group by i;
explain (costs off) select count(distinct d) from dqa_t1 group by i;

select count(distinct d), count(distinct c), count(distinct dt) from dqa_t1;
select count(distinct c), count(distinct dt), i from dqa_t1 group by i;


-- cleanup
drop table gp_dqa_t1;
drop table gp_dqa_t2;

create table foo_mdqa(x int, y int);

SELECT distinct C.z, count(distinct FS.x), count(distinct FS.y) FROM (SELECT 1 AS z FROM generate_series(1,10)) C, foo_mdqa FS GROUP BY z;

SELECT distinct C.z, count(distinct FS.x), count(distinct FS.y) FROM (SELECT i AS z FROM generate_series(1,10) i) C, foo_mdqa FS GROUP BY z;


drop table foo_mdqa;

-- non-strict agg test

-- Like COUNT(col), but also counts NULLs
create or replace function countall_trans(c int, newval int) returns int as $$
  SELECT $1 + 1;
$$ language sql;
create aggregate countall(sfunc = countall_trans, basetype = int, stype = int, initcond = 0, combinefunc = int4pl);

-- Test table
create table nonullstab (a int, b int);
insert into nonullstab select 1, 1 from generate_series(1, 100);

-- This returns wrong result. countall(distinct a) should return 1.
select countall(distinct a), count(distinct b) from nonullstab;

-- multi DQA with filter test
set enable_hashagg=on;
set enable_groupagg=off;

create table dqa_f1(a int, b int, c int) distributed by (a);
create table dqa_f2(x int, y int, z int) distributed by (x);

insert into dqa_f1 select i%17, i%5 , i%3 from generate_series(1,1000) i;
insert into dqa_f2 select i % 13, i % 5 , i % 11 from generate_series(1,1000) i;

select sum(distinct a) filter (where a > 0), sum(distinct b) filter (where a > 0) from dqa_f1;

select sum(distinct a) filter (where a > 0), sum(distinct b) filter (where a > 0) from dqa_f1 group by b;

select sum(distinct a) filter (where a > 0), sum(distinct b) filter (where a > 0) from dqa_f1 group by c;

select sum(distinct a) filter (where a in (select x from dqa_f2 where x = a)), sum(distinct b) filter (where a > 0) from dqa_f1;

select sum(distinct a) filter (where a in (select x from dqa_f2 where x = a)), sum(distinct b) filter (where a > 0) from dqa_f1 group by c;

select count(distinct a) filter (where a > 3),count( distinct b) filter (where a > 4), sum(distinct b) filter( where a > 4) from dqa_f1;

explain select sum(distinct a) filter (where a > 0), sum(distinct b) filter (where a > 0) from dqa_f1;

explain select sum(distinct a) filter (where a > 0), sum(distinct b) filter (where a > 0) from dqa_f1 group by b;

explain select sum(distinct a) filter (where a > 0), sum(distinct b) filter (where a > 0) from dqa_f1 group by c;

explain select sum(distinct a) filter (where a in (select x from dqa_f2 where x = a)), sum(distinct b) filter (where a > 0) from dqa_f1;

explain select sum(distinct a) filter (where a in (select x from dqa_f2 where x = a)), sum(distinct b) filter (where a > 0) from dqa_f1 group by c;

explain select count(distinct a) filter (where a > 3),count( distinct b) filter (where a > 4), sum(distinct b) filter( where a > 4) from dqa_f1;


-- single DQA with agg
-- the following SQL should use two stage agg
explain select count(distinct a), sum(b), sum(c) from dqa_f1;
select count(distinct a), sum(b), sum(c) from dqa_f1;
