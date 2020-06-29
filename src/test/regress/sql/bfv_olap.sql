-- Tests for old bugs related to OLAP queries.

-- First create a schema to contain the test tables, and few common test
-- tables that are shared by several test queries.
create schema bfv_olap;
set search_path=bfv_olap;

create table customer
(
  cn int not null,
  cname text not null,
  cloc text,

  primary key (cn)

) distributed by (cn);

insert into customer values
  ( 1, 'Macbeth', 'Inverness'),
  ( 2, 'Duncan', 'Forres'),
  ( 3, 'Lady Macbeth', 'Inverness'),
  ( 4, 'Witches, Inc', 'Lonely Heath');

create table vendor
(
  vn int not null,
  vname text not null,
  vloc text,

  primary key (vn)

) distributed by (vn);

insert into vendor values
  ( 10, 'Witches, Inc', 'Lonely Heath'),
  ( 20, 'Lady Macbeth', 'Inverness'),
  ( 30, 'Duncan', 'Forres'),
  ( 40, 'Macbeth', 'Inverness'),
  ( 50, 'Macduff', 'Fife');

create table sale
(
  cn int not null,
  vn int not null,
  pn int not null,
  dt date not null,
  qty int not null,
  prc float not null,

  primary key (cn, vn, pn)

) distributed by (cn,vn,pn);

insert into sale values
  ( 2, 40, 100, '1401-1-1', 1100, 2400),
  ( 1, 10, 200, '1401-3-1', 1, 0),
  ( 3, 40, 200, '1401-4-1', 1, 0),
  ( 1, 20, 100, '1401-5-1', 1, 0),
  ( 1, 30, 300, '1401-5-2', 1, 0),
  ( 1, 50, 400, '1401-6-1', 1, 0),
  ( 2, 50, 400, '1401-6-1', 1, 0),
  ( 1, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 600, '1401-6-1', 12, 5),
  ( 4, 40, 700, '1401-6-1', 1, 1),
  ( 4, 40, 800, '1401-6-1', 1, 1);


--
-- Test case errors out when we define aggregates without combine functions
-- and use it as an aggregate derived window function.
--

-- SETUP
-- start_ignore
drop table if exists toy;
drop aggregate if exists mysum1(int4);
drop aggregate if exists mysum2(int4);
-- end_ignore
create table toy(id,val) as select i,i from generate_series(1,5) i;
create aggregate mysum1(int4) (sfunc = int4_sum, combinefunc=int8pl, stype=bigint);
create aggregate mysum2(int4) (sfunc = int4_sum, stype=bigint);

-- TEST
select id, val, sum(val) over (w), mysum1(val) over (w), mysum2(val) over (w) from toy window w as (order by id rows 2 preceding);

-- CLEANUP
-- start_ignore
drop aggregate if exists mysum1(int4);
drop aggregate if exists mysum2(int4);
drop table if exists toy;
-- end_ignore

--
-- Test case errors out when we define aggregates without preliminary functions and use it as an aggregate derived window function.
--

-- SETUP
-- start_ignore
drop type if exists ema_type cascade;
drop function if exists ema_adv(t ema_type, v float, x float) cascade;
drop function if exists ema_fin(t ema_type) cascade;
drop aggregate if exists ema(float, float);
drop table if exists ema_test cascade;
-- end_ignore
create type ema_type as (x float, e float);

create function ema_adv(t ema_type, v float, x float)
    returns ema_type
    as $$
        begin
            if t.e is null then
                t.e = v;
                t.x = x;
            else
                if t.x != x then
                    raise exception 'ema smoothing x may not vary';
                end if;
                t.e = t.e + (v - t.e) * t.x;
            end if;
            return t;
        end;
    $$ language plpgsql;

create function ema_fin(t ema_type)
    returns float
    as $$
       begin
           return t.e;
       end;
    $$ language plpgsql;

create aggregate ema(float, float) (
    sfunc = ema_adv,
    stype = ema_type,
    finalfunc = ema_fin,
    initcond = '(,)');

create table ema_test (k int, v float ) distributed by (k);
insert into ema_test select i, 4*(i::float/20) + 10.0*(1+cos(radians(i*5))) from generate_series(0,19) i(i);

-- TEST
select k, v, ema(v, 0.9) over (order by k) from ema_test order by k;
select k, v, ema(v, 0.9) over (order by k rows between unbounded preceding and current row) from ema_test order by k;

-- CLEANUP
-- start_ignore
drop table if exists ema_test cascade;
drop aggregate if exists ema(float, float);
drop function if exists ema_fin(t ema_type) cascade;
drop function if exists ema_adv(t ema_type, v float, x float) cascade;
drop type if exists ema_type cascade;
-- end_ignore


--
-- Test with/without group by
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS r;
-- end_ignore
CREATE TABLE r
(
    a INT NOT NULL, 
    b INT, 
    c CHARACTER VARYING(200),  
    d NUMERIC(10,0), 
    e DATE
) DISTRIBUTED BY (a,b);
ALTER TABLE r ADD CONSTRAINT PKEY PRIMARY KEY (b);

--TEST
SELECT MAX(a) AS m FROM r GROUP BY b ORDER BY m;
SELECT MAX(a) AS m FROM r GROUP BY a ORDER BY m;
SELECT MAX(a) AS m FROM r GROUP BY b;

-- ORDER BY clause includes some grouping column or not
SELECT MAX(a) AS m FROM R GROUP BY b ORDER BY m,b;
SELECT MAX(a) AS m FROM R GROUP BY b,e ORDER BY m,b,e;
SELECT MAX(a) AS m FROM R GROUP BY b,e ORDER BY m;

-- ORDER BY 1 or more columns
SELECT MAX(a),d,e AS m FROM r GROUP BY b,d,e ORDER BY m,e,d;
SELECT MIN(a),d,e AS m FROM r GROUP BY b,e,d ORDER BY e,d;
SELECT MAX(a) AS m FROM r GROUP BY b,c,d,e ORDER BY e,d;
SELECT MAX(a) AS m FROM r GROUP BY b,e ORDER BY e;
SELECT MAX(e) AS m FROM r GROUP BY b ORDER BY m;

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS r;
-- end_ignore

--
-- ORDER BY clause includes some grouping column or not
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS dm_calendar;
-- end_ignore
CREATE TABLE dm_calendar (
    calendar_id bigint NOT NULL,
    date_name character varying(200),
    date_name_cn character varying(200),
    calendar_date date,
    current_day numeric(10,0),
    month_id numeric(10,0),
    month_name character varying(200),
    month_name_cn character varying(200),
    month_name_short character varying(200),
    month_name_short_cn character varying(200),
    days_in_month numeric(10,0),
    first_of_month numeric(10,0),
    last_month_id numeric(10,0),
    month_end numeric(10,0),
    quarter_id numeric(10,0),
    quarter_name character varying(200),
    quarter_name_cn character varying(200),
    quarter_name_short character varying(200),
    quarter_name_short_cn character varying(200),
    year_id numeric(10,0),
    year_name character varying(200),
    year_name_cn character varying(200),
    description character varying(500),
    create_date timestamp without time zone,
    month_week_num character varying(100),
    month_week_begin character varying(100),
    month_week_end character varying(100),
    half_year character varying(100),
    weekend_flag character varying(100),
    holidays_flag character varying(100),
    workday_flag character varying(100),
    month_number numeric(10,0)
) DISTRIBUTED BY (calendar_id);
ALTER TABLE ONLY dm_calendar ADD CONSTRAINT dm_calendar_pkey PRIMARY KEY (calendar_id);

--TEST
SELECT "year_id" as id , min("year_name") as a  from (select "year_id" as "year_id" , min("year_name") as "year_name" from  "dm_calendar" group by "year_id") "dm_calendar3" group by "year_id" order by a ASC ;

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS dm_calendar;
-- end_ignore


--
-- Test with/without group by with primary key as dist key
--

-- SETUP
-- start_ignore
drop table if exists t;
-- end_ignore
create table t
(
    a int NOT NULL,
    b int,
    c character varying(200),
    d numeric(10,0),
    e date
) distributed by (b);
alter table t ADD CONSTRAINT pkey primary key (b);

-- TEST
SELECT MAX(a) AS m FROM t GROUP BY b ORDER BY m;

-- CLEANUP
-- start_ignore
drop table if exists t;
-- end_ignore




--
-- Passing through distribution matching type in default implementation
--
  
-- TEST
select cname,
rank() over (partition by sale.cn order by vn)
from sale, customer
where sale.cn = customer.cn
order by 1, 2;


--
-- Optimizer query crashing for logical window with no window functions
--

-- SETUP
create table mpp23240(a int, b int, c int, d int, e int, f int);

-- TEST
select a, b,
       case 1
        when 10 then
          sum(c) over(partition by a)
        when 20 then
          sum(d) over(partition by a)
        else
          5
       end as sum1
from (select * from mpp23240 where f > 10) x;

-- CLEANUP
-- start_ignore
drop table mpp23240;
-- end_ignore


--
-- Test for the bug reported at https://github.com/greenplum-db/gpdb/issues/2236
--
create table test1 (x int, y int, z double precision);
insert into test1 select a, b, a*10 + b from generate_series(1, 5) a, generate_series(1, 5) b;

select sum(z) over (partition by x) as sumx, sum(z) over (partition by y) as sumy from test1;

drop table test1;


--
-- This failed at one point because of an over-zealous syntax check, with
-- "window functions not allowed in WHERE clause" error.
--
select sum(g) from generate_series(1, 5) g
where g in (
  select rank() over (order by x) from generate_series(1,5) x
);


--
-- This caused a crash in ROLLUP planning at one point.
--
SELECT sale.vn
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP( (sale.dt,sale.cn),(sale.pn),(sale.vn));

SELECT DISTINCT sale.vn
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP( (sale.dt,sale.cn),(sale.pn),(sale.vn));


--
-- Another ROLLUP query, that hit a bug in setting up the planner-generated
-- subquery's targetlist. (https://github.com/greenplum-db/gpdb/issues/6754)
--
SELECT sale.vn, rank() over (partition by sale.vn)
FROM vendor, sale
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP( sale.vn);


--
-- Test window function with constant PARTITION BY
--
CREATE TABLE testtab (a int4);
insert into testtab values (1), (2);
SELECT count(*) OVER (PARTITION BY 1) AS count FROM testtab;

-- Another variant, where the PARTITION BY is not a literal, but the
-- planner can deduce that it's a constant through equivalence classes.
SELECT 1
FROM (
  SELECT a, count(*) OVER (PARTITION BY a) FROM (VALUES (1,1)) AS foo(a)
) AS sup(c, d)
WHERE c = 87 ;

--
-- This used to crash, and/or produce incorrect results. The culprit was that a Hash Agg
-- was used, but the planner put a Gather Merge at the top, without a Sort, even though
-- a Hash Agg doesn't preserve the sort order.
--
SELECT sale.qty
FROM sale
GROUP BY ROLLUP((qty)) order by 1;

--
-- Test two-stage aggregate with grouping sets and a HAVING clause
--

-- persuade planner to choose a two-stage plan.
set gp_motion_cost_per_row TO 1000;
select cn, sum(qty) from sale group by rollup(cn,vn) having sum(qty)=1;
-- same, but the HAVING clause matches a rolled up row.
select cn, sum(qty) from sale group by rollup(cn,vn) having sum(qty)=1144;


--
-- Test a query with window function over an aggregate, and a subquery.
--
-- Github Issue https://github.com/greenplum-db/gpdb/issues/10143
create table t1_github_issue_10143(
  base_ym varchar(6),
  code varchar(5),
  name varchar(60)
);

create table t2_github_issue_10143(
  base_ym varchar(6),
  dong varchar(8),
  code varchar(6),
  salary numeric(18)
);

insert into t1_github_issue_10143 values ('a', 'acode', 'aname');
insert into t2_github_issue_10143 values ('a', 'adong', 'acode', 1000);
insert into t2_github_issue_10143 values ('b', 'bdong', 'bcode', 1100);

set optimizer_trace_fallback = on;

explain select (select name from t1_github_issue_10143 where code = a.code limit 1) as dongnm
,sum(sum(a.salary)) over()
from t2_github_issue_10143 a
group by a.code;

select (select name from t1_github_issue_10143 where code = a.code limit 1) as dongnm
,sum(sum(a.salary)) over()
from t2_github_issue_10143 a
group by a.code;

select * from (select sum(a.salary) over(), count(*)
               from t2_github_issue_10143 a
               group by a.salary) T;

-- this query currently falls back, needs to be fixed
select (select rn from (select row_number() over () as rn, name
                        from t1_github_issue_10143
                        where code = a.code
                        group by name) T
       ) as dongnm
,sum(sum(a.salary)) over()
from t2_github_issue_10143 a
group by a.code;

with cte as (select row_number() over (order by code) as rn1, code
             from t2_github_issue_10143
             group by code)
select row_number() over (order by name) as rn2, name
from t1_github_issue_10143
group by name
union all
select * from cte;

reset optimizer_trace_fallback;

-- CLEANUP
-- start_ignore
drop schema bfv_olap cascade;
-- end_ignore

