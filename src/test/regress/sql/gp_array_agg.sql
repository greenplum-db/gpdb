--
-- Test parallel array_agg(anynonarray) and array_agg(anyarray)
--

create schema test_gp_array_agg;
set search_path=test_gp_array_agg;
set optimizer_trace_fallback = on;

-- Test array_agg(anynonarray)
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

-- Test array_agg(anyarray)
create table int_array_table (a int, arr int[]);
insert into int_array_table select i, array[i, i] from generate_series(1, 5)i;
insert into int_array_table select 6, '{6, NULL}'::int[];
insert into int_array_table select 7, '{6, NULL}'::int[];
insert into int_array_table select 8, '{NULL, 7}'::int[];
insert into int_array_table select 9, '{NULL, 7}'::int[];
insert into int_array_table select NULL, '{NULL, NULL}'::int[];

select gp_array_agg(arr order by arr) from int_array_table;
select gp_array_agg(arr order by arr desc) from int_array_table;
select gp_array_agg(arr order by a nulls first) from int_array_table;
select gp_array_agg(arr order by a nulls last) from int_array_table;
select gp_array_agg(arr order by a desc nulls first) from int_array_table;
select gp_array_agg(arr order by a desc nulls last) from int_array_table;
select a, gp_array_agg(arr order by arr) from int_array_table group by a order by a;

select array_dims(gp_array_agg(distinct arr)) from int_array_table;
explain (verbose, costs off) select array_dims(gp_array_agg(distinct arr)) from int_array_table;

CREATE TABLE arrtest (
    a        int2[],
    b        int4[][][],
    c        name[],
    d        text[][],
    e        float8[],
    f        char(5)[],
    g        varchar(5)[]
) DISTRIBUTED RANDOMLY;

INSERT INTO arrtest (a, b[1:2][1:2][1:2], c, d, e, f, g)
VALUES ('{1,2}', '{{{0,0},{1,2}},{{3,4},{5,6}}}', '{"foo"}',
        '{{"elt1", "elt2"}}', '{1.1, 2.2}',
        '{"abc","abcde"}', '{"abc","abcde"}');
INSERT INTO arrtest (a, b[1:2][1:2][1:2], c, d, e, f, g)
VALUES ('{1,2}', '{{{7,8},{9,10}},{{11,12},{13,14}}}', '{"bar"}',
        '{{"elt1", "elt2"}}', '{"3.3", "4.4"}',
        '{"abc","abcde"}', '{"abc","abcde"}');

SELECT $query$
select
  array_agg(a) agg_a,
  array_dims(array_agg(b)) dims_b,
  array_dims(array_agg(c)) dims_c,
  array_agg(d) agg_d,
  array_dims(array_agg(e)) dims_e,
  array_agg(f) agg_f,
  array_agg(g) agg_g
from arrtest;
$query$ AS qry \gset

EXPLAIN (COSTS OFF, VERBOSE)
:qry ;

:qry ;

-- CLEANUP
drop schema test_gp_array_agg cascade;
reset optimizer_trace_fallback;
