drop schema if exists qf cascade;
create schema qf;

-- It used to fail because query cancel is sent and QE goes out of
-- transaction block, though QD sends 2PC request.
begin;
drop table if exists qf.foo;
create table qf.foo as select i a, i b from generate_series(1, 100)i;
select case when gp_segment_id = 0 then pg_sleep(3) end from qf.foo limit 1;
commit;

-- with order by.  CTE can prevent LIMIT from being pushed down.
begin;
drop table if exists qf.bar2;
create table qf.bar2 as select i a, i b from generate_series(1, 100)i;
with t2 as(
	select * from skewed_lineitem
	order by l_orderkey
)
select * from t2 order by 1,2,3,4,5 limit 1;
commit;

-- with window function.
begin;
drop table if exists qf.bar3;
create table qf.bar3(a int, b text, c numeric) with (appendonly=true);
insert into qf.bar3 select a, repeat('x', 10) b, b from qf.bar2;
with t3 as(
	select
		l_skewkey,
		count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
	from skewed_lineitem
)
select * from t3 order by 1,2 limit 2;
commit;

-- combination.
begin;
drop table if exists qf.bar4;
create table qf.bar4(a int, b int, c text) with (appendonly=true, orientation=column);
insert into qf.bar4 select a, b, 'foo' from qf.bar2;
with t4a as(
	select
		l_skewkey,
		count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
	from skewed_lineitem
), t4b as (
	select * from skewed_lineitem
	order by l_orderkey
)
select a.l_skewkey, b.l_skewkey from t4a a
inner join t4b b on a.l_skewkey = b.l_skewkey
limit 3;
commit;

-- median.
begin;
drop table if exists qf.bar5;
create table qf.bar5(a int, b int);
insert into qf.bar5 select i, i % 10 from generate_series(1, 10)i;
with t5a as(
	select
		l_skewkey,
		median(l_quantity) med
	from skewed_lineitem
	group by l_skewkey
), t5b as (
	select * from skewed_lineitem
	order by l_orderkey
)
select a.l_skewkey, a.med from t5a a
inner join t5b b on a.l_skewkey = b.l_skewkey order by a.l_skewkey, a.med
limit 1;
commit;

--Combination median and windows

begin;

with t3 as(
    select
        l_skewkey,
        count(*) over (partition by l_skewkey order by l_quantity, l_orderkey)
    from skewed_lineitem
),

t4 as ( select
        l_skewkey,
        median(l_quantity) med
    from skewed_lineitem
    group by l_skewkey
)
select a.l_skewkey from t3 a  left outer join t4 b  on a. l_skewkey  = b. l_skewkey order by a.l_skewkey  limit 1;

commit;

--csq

begin;
select l_returnflag  from skewed_lineitem  t1 where l_skewkey in (select l_skewkey from  skewed_lineitem t2 where t1.l_shipinstruct = t2.l_shipinstruct) order by l_returnflag limit 3;
commit;
