drop extension if exists gp_array_agg;
create extension gp_array_agg;
create table perct as select a, a / 10 as b from generate_series(1, 100)a distributed by (a);
drop table if exists t1;
create table t1 (a varchar, b character varying) distributed randomly;
insert into t1 values ('aaaaaaa', 'cccccccccc');
insert into t1 values ('aaaaaaa', 'ddddd');
insert into t1 values ('bbbbbbb', 'eeee');
insert into t1 values ('bbbbbbb', 'eeef');
insert into t1 values ('bbbbb', 'dfafa');
create temporary table aggordertest (a int4, b int4) distributed by (a);
insert into aggordertest values (1,1), (2,2), (1,3), (3,4), (null,5), (2,null);
create table mergeappend_test ( a int, b int, x int ) distributed by (a,b);
insert into mergeappend_test select g/100, g/100, g from generate_series(1, 500) g;
analyze mergeappend_test;
create table pagg_test (x int, y int);
insert into pagg_test
select (case x % 4 when 1 then null else x end), x % 10
from generate_series(1,5000) x;


select (select gp_array_agg(a order by a) from perct where median(t.a) = 50.5) from (select * from perct t order by a offset 0) as t;
select gp_array_agg(f order by f)  from (select b::text as f from t1 group by b order by b) q;
select gp_array_agg(a order by a nulls first) from aggordertest;
select gp_array_agg(a order by a nulls last) from aggordertest;
select gp_array_agg(a order by a desc nulls first) from aggordertest;
select gp_array_agg(a order by a desc nulls last) from aggordertest;
select gp_array_agg(a order by b nulls first) from aggordertest;
select gp_array_agg(a order by b nulls last) from aggordertest;
select gp_array_agg(a order by b desc nulls first) from aggordertest;
select gp_array_agg(a order by b desc nulls last) from aggordertest;
select a, b, array_dims(gp_array_agg(x)) from mergeappend_test r group by a, b
union all
select null, null, array_dims(gp_array_agg(x)) from mergeappend_test r
order by 1,2;

explain (costs off)
select a, b, array_dims(gp_array_agg(x)) from mergeappend_test r group by a, b
union all
select null, null, array_dims(gp_array_agg(x)) from mergeappend_test r
order by 1,2;

select a, b, array_dims(gp_array_agg(x)) from mergeappend_test r group by a, b
union all
select null, null, array_dims(gp_array_agg(x)) from mergeappend_test r, pg_sleep(0)
order by 1,2;

explain analyze select a, b, array_dims(gp_array_agg(x)) from mergeappend_test r group by a, b
union all
select null, null, array_dims(gp_array_agg(x)) from mergeappend_test r
order by 1,2;

-- create a view as we otherwise have to repeat this query a few times.
create view v_pagg_test as
select
	y,
	min(t) as tmin,max(t) as tmax,count(distinct t) as tndistinct,
	min(a) as amin,max(a) as amax,count(distinct a) as andistinct
from (
	select
		y,
		unnest(regexp_split_to_array(a1.t, ','))::int as t,
		unnest(a1.a) as a
	from (
		select
			y,
			string_agg(x::text, ',') as t,
			string_agg(x::text::bytea, ',') as b,
			gp_array_agg(x) as a
		from pagg_test
		group by y
	) a1
) a2
group by y;

-- ensure results are correct.
select * from v_pagg_test order by y;

explain (costs off) select * from v_pagg_test order by y;
