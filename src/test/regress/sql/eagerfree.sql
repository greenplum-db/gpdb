create schema eagerfree;
set search_path=eagerfree;

create table smallt (i int, t text, d date) distributed by (i);
insert into smallt select i%10, 'text ' || (i%15), '2011-01-01'::date + ((i%20) || ' days')::interval
from generate_series(0, 99) i;
analyze smallt;

create table bigt (i int, t text, d date) distributed by (i);
insert into bigt select i/10, 'text ' || (i/15), '2011-01-01'::date + ((i/20) || ' days')::interval
from generate_series(0, 999999) i;
analyze bigt;

create table smallt2 (i int, t text, d date) distributed by (i);
insert into smallt2 select i%5, 'text ' || (i%10), '2011-01-01'::date + ((i%15) || ' days')::interval
from generate_series(0, 49) i;
analyze smallt2;

set optimizer_segments = 3;
set gp_motion_cost_per_row = 0.1;

-- HashAgg, Agg
select d, count(*) from smallt group by d;
explain analyze select d, count(*) from smallt group by d;

select * from
  test_util.extract_plan_stats($$
select d, count(*) from smallt group by d;
  $$, false)
where stats_name = 'executor_mem_lines'
or stats_name = 'workmem_wanted_lines'
order by stats_name;

set statement_mem=2560;
select count(*) from (select i, t, d, count(*) from bigt group by i, t, d) tmp;
explain analyze select count(*) from (select i, t, d, count(*) from bigt group by i, t, d) tmp;
set statement_mem=128000;

-- DQA
set gp_enable_agg_distinct=off;
set gp_enable_multiphase_agg=off;
select count(distinct d) from smallt;
explain analyze select count(distinct d) from smallt;

set statement_mem=2560;
select count(distinct d) from bigt;
explain analyze select count(distinct d) from bigt;
set statement_mem=128000;

set gp_enable_agg_distinct=on;
set gp_enable_multiphase_agg=on;

-- Rescan on Agg (with Material in the inner side of nestloop)
-- start_ignore
-- Known_opt_diff: This test is only here for the planner. It doesn't exercise eagerfree in ORCA plans
-- end_ignore
set enable_nestloop=on;
set enable_hashjoin=off;
select t1.*, t2.* from
(select d, count(*) from smallt group by d) as t1, (select d, sum(i) from smallt group by d) as t2
where t1.d = t2.d;
explain analyze select t1.*, t2.* from
(select d, count(*) from smallt group by d) as t1, (select d, sum(i) from smallt group by d) as t2
where t1.d = t2.d;

select * from
  test_util.extract_plan_stats($$
select t1.*, t2.* from
(select d, count(*) from smallt group by d) as t1, (select d, sum(i) from smallt group by d) as t2
where t1.d = t2.d;
  $$, false)
where stats_name = 'executor_mem_lines'
or stats_name = 'workmem_wanted_lines'
order by stats_name;

set enable_nestloop=off;
set enable_hashjoin=on;

-- Rescan on Agg (with Material in the inner side of nestloop)
-- start_ignore
-- Known_opt_diff: This test is only here for the planner. It doesn't exercise eagerfree in ORCA plans
-- end_ignore
set enable_nestloop=on;
set enable_hashjoin=off;
select t1.*, t2.* from
(select i, count(*) from smallt group by i) as t1, (select i, sum(i) from smallt group by i) as t2
where t1.i = t2.i;
explain analyze select t1.*, t2.* from
(select i, count(*) from smallt group by i) as t1, (select i, sum(i) from smallt group by i) as t2
where t1.i = t2.i;

select * from
  test_util.extract_plan_stats($$
select t1.*, t2.* from
(select i, count(*) from smallt group by i) as t1, (select i, sum(i) from smallt group by i) as t2
where t1.i = t2.i;
  $$, false)
where stats_name = 'executor_mem_lines'
or stats_name = 'workmem_wanted_lines'
order by stats_name;

set enable_nestloop=off;
set enable_hashjoin=on;

-- Limit on Agg
select d, count(*) from smallt group by d limit 5; --ignore
explain analyze select d, count(*) from smallt group by d limit 5;

select * from
  test_util.extract_plan_stats($$
select d, count(*) from smallt group by d limit 5;
  $$, false)
where stats_name = 'executor_mem_lines'
or stats_name = 'workmem_wanted_lines'
order by stats_name;

-- HashJoin
select t1.* from smallt as t1, smallt as t2 where t1.i = t2.i order by 1,2,3;
explain analyze select t1.* from smallt as t1, smallt as t2 where t1.i = t2.i;

-- Rescan on HashJoin
--select t1.* from (select t11.* from smallt as t11, smallt as t22 where t11.i = t22.i and t11.i < 2) as t1,
--   (select t11.* from smallt as t11, smallt as t22 where t11.d = t22.d and t11.i < 5) as t2;

-- Material in SubPlan
select smallt2.* from smallt2
where i < (select count(*) from smallt where smallt.i = smallt2.i) order by 1,2,3;
explain select smallt2.* from smallt2
where i < (select count(*) from smallt where smallt.i = smallt2.i);

-- Sort in MergeJoin
-- start_ignore
-- Known_opt_diff: OPT-3417. Missing feature: Sort Merge Join
-- end_ignore
set enable_hashjoin=off;
set enable_mergejoin=on;
select t1.* from smallt as t1, smallt as t2 where t1.i = t2.i and t1.i < 2;
explain analyze select t1.* from smallt as t1, smallt as t2 where t1.i = t2.i and t1.i < 2;

select t1.* from smallt as t1, smallt as t2 where t1.d = t2.d and t1.i < 2;
--start_ignore
explain analyze select t1.* from smallt as t1, smallt as t2 where t1.d = t2.d and t1.i < 2;
--end_ignore
set enable_hashjoin=on;
set enable_mergejoin=off;

-- ShareInputScan
--with my_group_max(i, maximum) as (select i, max(d) from smallt group by i)
--select smallt2.* from my_group_max, smallt2 where my_group_max.i = smallt2.i
--and smallt2.i < any (select maximum from my_group_max);
--explain analyze with my_group_max(i, maximum) as (select i, max(d) from smallt group by i)
--select smallt2.* from my_group_max, smallt2 where my_group_max.i = smallt2.i
--and smallt2.i < any (select maximum from my_group_max);

-- IndexScan
create index smallt_d_idx on smallt (d);
create index smallt2_d_idx on smallt2 (d);

-- start_ignore
-- Known_opt_diff: This test is only here for the planner. It doesn't exercise eagerfree in ORCA plans
-- end_ignore
set enable_hashjoin=off;
set enable_nestloop=on;
set enable_seqscan=off;
set enable_bitmapscan=off;
select smallt.* from smallt, smallt2 where smallt.i = smallt2.i and smallt2.d = '2011-01-04'::date
and smallt.d = '2011-01-04'::date order by 1,2,3;
explain analyze select smallt.* from smallt, smallt2 where smallt.i = smallt2.i and smallt2.d = '2011-01-04'::date
and smallt.d = '2011-01-04'::date;

-- IndexOnlyScan
explain analyze select *, exists(select 1 from pg_class where oid = c.oid) as dummy from pg_class c;

-- BitmapScan
-- start_ignore
-- Known_opt_diff: This test is only here for the planner. It doesn't exercise eagerfree in ORCA plans
-- end_ignore
set enable_indexscan=off;
set enable_bitmapscan=on;
select smallt.* from smallt, smallt2 where smallt.i = smallt2.i and smallt2.d = '2011-01-04'::date
and smallt.d = '2011-01-04'::date order by 1,2,3;
explain analyze select smallt.* from smallt, smallt2 where smallt.i = smallt2.i and smallt2.d = '2011-01-04'::date
and smallt.d = '2011-01-04'::date;
set enable_hashjoin=on;
set enable_nestloop=off;
set enable_seqscan=on;
set enable_indexscan=on;

-- SubPlan
with my_group_sum(d, total) as (select d, sum(i) from smallt group by d)
select smallt2.* from smallt2
where i < all (select total from my_group_sum, smallt, smallt2 as tmp where my_group_sum.d = smallt.d and smallt.d = tmp.d and my_group_sum.d = smallt2.d)
and i = 0 order by 1,2,3; --order 1,2,3

select smallt2.* from smallt2
where i < all (select total from (select d, sum(i) as total from smallt group by d) as my_group_sum, smallt, smallt2 as tmp
    where my_group_sum.d = smallt.d and smallt.d = tmp.d and my_group_sum.d = smallt2.d)
and i = 0 order by 1,2,3; --order 1,2,3

-- Test Subplan with Agg
with my_group_sum(d, total) as (select d, sum(i) from smallt group by d)
select count(*) from smallt2
where 0 < all (select total from my_group_sum, smallt2 as tmp where my_group_sum.d = smallt2.d);
--start_ignore
explain with my_group_sum(d, total) as (select d, sum(i) from smallt group by d)
        select count(*) from smallt2
        where 0 < all (select total from my_group_sum, smallt2 as tmp where my_group_sum.d = smallt2.d);
--end_ignore

-- Nested Subplan
create table eager_free_r (r1 int, r2 int, r3 int);
create table eager_free_s (s1 int, s2 int, s3 int);
create table eager_free_t (t1 int, t2 int, t3 int);
insert into eager_free_r select 1 + g % 20, 1 + g % 5, 1 + g % 8 from generate_series(0, 39) g;
insert into eager_free_s select 1 + g % 20, 6 + g % 5, 1 + g % 4 from generate_series(0, 19) g;
insert into eager_free_t select 1 + g % 30, 1 + g % 6, 1 + g % 5 from generate_series(0, 29) g;

select * from eager_free_t where t1 > (select min(r1) from eager_free_r where r2<t2 and r3 > (Select min(s3) from eager_free_s where s1<r1));
reset optimizer_segments;

reset search_path;
drop schema eagerfree cascade;
