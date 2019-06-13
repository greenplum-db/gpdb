-- start_ignore
create schema subselect_gp;
set search_path to subselect_gp, public;
-- end_ignore
set optimizer_enable_master_only_queries = on;
set optimizer_segments = 3;
set optimizer_nestloop_factor = 1.0;

--
-- Base tables for CSQ tests
--

drop table if exists csq_t1_base;
create table csq_t1_base(x int, y int) distributed by (x);

insert into csq_t1_base values(1,2);
insert into csq_t1_base values(2,1);
insert into csq_t1_base values(4,2);

drop table if exists csq_t2_base;
create table csq_t2_base(x int, y int) distributed by (x);

insert into csq_t2_base values(3,2);
insert into csq_t2_base values(3,2);
insert into csq_t2_base values(3,2);
insert into csq_t2_base values(3,2);
insert into csq_t2_base values(3,1);


--
-- Correlated subqueries
--

drop table if exists csq_t1;
drop table if exists csq_t2;

create table csq_t1(x int, y int) distributed by (x);
create table csq_t2(x int, y int) distributed by (x);

insert into csq_t1 select * from csq_t1_base;
insert into csq_t2 select * from csq_t2_base;

select * from csq_t1 where csq_t1.x >ALL (select csq_t2.x from csq_t2 where csq_t2.y=csq_t1.y) order by 1; -- expected (4,2)

--
-- correlations in the targetlist
--

select csq_t1.x, (select sum(bar.x) from csq_t1 bar where bar.x >= csq_t1.x) as sum from csq_t1 order by csq_t1.x;

select csq_t1.x, (select sum(bar.x) from csq_t1 bar where bar.x = csq_t1.x) as sum from csq_t1 order by csq_t1.x;

select csq_t1.x, (select bar.x from csq_t1 bar where bar.x = csq_t1.x) as sum from csq_t1 order by csq_t1.x;

--
-- CSQs with partitioned tables
--

drop table if exists csq_t1;
drop table if exists csq_t2;

create table csq_t1(x int, y int) 
distributed by (x)
partition by range (y) ( start (0) end (4) every (1))
;

create table csq_t2(x int, y int) 
distributed by (x)
partition by range (y) ( start (0) end (4) every (1))
;

insert into csq_t1 select * from csq_t1_base;
insert into csq_t2 select * from csq_t2_base;

explain select * from csq_t1 where csq_t1.x >ALL (select csq_t2.x from csq_t2 where csq_t2.y=csq_t1.y) order by 1;

select * from csq_t1 where csq_t1.x >ALL (select csq_t2.x from csq_t2 where csq_t2.y=csq_t1.y) order by 1; -- expected (4,2)

drop table if exists csq_t1;
drop table if exists csq_t2;
drop table if exists csq_t1_base;
drop table if exists csq_t2_base;

--
-- Multi-row subqueries
--

drop table if exists mrs_t1;
create table mrs_t1(x int) distributed by (x);

insert into mrs_t1 select generate_series(1,20);

explain select * from mrs_t1 where exists (select x from mrs_t1 where x < -1);
select * from mrs_t1 where exists (select x from mrs_t1 where x < -1) order by 1;

explain select * from mrs_t1 where exists (select x from mrs_t1 where x = 1);
select * from mrs_t1 where exists (select x from mrs_t1 where x = 1) order by 1;

explain select * from mrs_t1 where x in (select x-95 from mrs_t1) or x < 5;
select * from mrs_t1 where x in (select x-95 from mrs_t1) or x < 5 order by 1;

drop table if exists mrs_t1;

--
-- Multi-row subquery from MSTR
--
drop table if exists mrs_u1;
drop table if exists mrs_u2;

create TABLE mrs_u1 (a int, b int);
create TABLE mrs_u2 (a int, b int);

insert into mrs_u1 values (1,2),(11,22);
insert into mrs_u2 values (1,2),(11,22),(33,44);

select * from mrs_u1 join mrs_u2 on mrs_u1.a=mrs_u2.a where mrs_u1.a in (1,11) or mrs_u2.a in (select a from mrs_u1 where a=1) order by 1;

drop table if exists mrs_u1;
drop table if exists mrs_u2;

--
-- MPP-13758
--

drop table if exists csq_m1;
create table csq_m1();
set allow_system_table_mods=true;
delete from gp_distribution_policy where localoid='csq_m1'::regclass;
reset allow_system_table_mods;
alter table csq_m1 add column x int;
insert into csq_m1 values(1);

drop table if exists csq_d1;
create table csq_d1(x int) distributed by (x);
insert into csq_d1 select * from csq_m1;

explain select array(select x from csq_m1); -- no initplan
select array(select x from csq_m1); -- {1}

explain select array(select x from csq_d1); -- initplan
select array(select x from csq_d1); -- {1}

--
-- CSQs involving master-only and distributed tables
--

drop table if exists t3cozlib;

create table t3cozlib (c1 int , c2 varchar) with (appendonly=true, compresstype=zlib, orientation=column) distributed by (c1);

drop table if exists pg_attribute_storage;

create table pg_attribute_storage (attrelid int, attnum int, attoptions text[]) distributed by (attrelid);

insert into pg_attribute_storage values ('t3cozlib'::regclass, 1, E'{\'something\'}');
insert into pg_attribute_storage values ('t3cozlib'::regclass, 2, E'{\'something2\'}');

SELECT a.attname
, pg_catalog.format_type(a.atttypid, a.atttypmod)

, ( SELECT substring(pg_catalog.pg_get_expr(d.adbin, d.adrelid) for 128)
 FROM pg_catalog.pg_attrdef d
WHERE d.adrelid = a.attrelid AND d.adnum = a.attnum AND a.atthasdef
)
, a.attnotnull
, a.attnum
, a.attstorage
, pg_catalog.col_description(a.attrelid, a.attnum)
, ( SELECT s.attoptions
FROM pg_attribute_storage s
WHERE s.attrelid = a.attrelid AND s.attnum = a.attnum
) newcolumn

FROM pg_catalog.pg_attribute a
WHERE a.attrelid = 't3cozlib'::regclass AND a.attnum > 0 AND NOT a.attisdropped
ORDER BY a.attnum
; -- expect to see 2 rows

--
-- More CSQs involving master-only and distributed relations
--

drop table if exists csq_m1;
create table csq_m1();
set allow_system_table_mods=true;
delete from gp_distribution_policy where localoid='csq_m1'::regclass;
reset allow_system_table_mods;
alter table csq_m1 add column x int;
insert into csq_m1 values(1),(2),(3);

drop table if exists csq_d1;
create table csq_d1(x int) distributed by (x);
insert into csq_d1 select * from csq_m1 where x < 3;
insert into csq_d1 values(4);

select * from csq_m1;
select * from csq_d1;

--
-- outer plan node is master-only and CSQ has distributed relation
--

explain select * from csq_m1 where x not in (select x from csq_d1) or x < -100; -- gather motion
select * from csq_m1 where x not in (select x from csq_d1) or x < -100; -- (3)

--
-- outer plan node is master-only and CSQ has distributed relation
--

explain select * from csq_d1 where x not in (select x from csq_m1) or x < -100; -- broadcast motion
select * from csq_d1 where x not in (select x from csq_m1) or x < -100; -- (4)

-- drop csq_m1 since we deleted its gp_distribution_policy entry
drop table csq_m1;

--
-- MPP-14441 Don't lose track of initplans
--
drop table if exists csq_t1;
CREATE TABLE csq_t1 (a int, b int, c int, d int, e text) DISTRIBUTED BY (a);
INSERT INTO csq_t1 SELECT i, i/3, i%2, 100-i, 'text'||i  FROM generate_series(1,100) i;

select count(*) from csq_t1 t1 where a > (SELECT x.b FROM ( select avg(a)::int as b,'haha'::text from csq_t1 t2 where t2.a=t1.d) x ) ;

select count(*) from csq_t1 t1 where a > ( select avg(a)::int from csq_t1 t2 where t2.a=t1.d) ;

--
-- correlation in a func expr
--
CREATE OR REPLACE FUNCTION csq_f(a int) RETURNS int AS $$ select $1 $$ LANGUAGE SQL CONTAINS SQL;
DROP TABLE IF EXISTS csq_r;
CREATE TABLE csq_r(a int) distributed by (a);
INSERT INTO csq_r VALUES (1);

-- subqueries shouldn't be pulled into a join if the from clause has a function call
-- with a correlated argument

-- force_explain
explain SELECT * FROM csq_r WHERE a IN (SELECT * FROM csq_f(csq_r.a));

SELECT * FROM csq_r WHERE a IN (SELECT * FROM csq_f(csq_r.a));

-- force_explain
explain SELECT * FROM csq_r WHERE a not IN (SELECT * FROM csq_f(csq_r.a));

SELECT * FROM csq_r WHERE a not IN (SELECT * FROM csq_f(csq_r.a));

-- force_explain
explain SELECT * FROM csq_r WHERE exists (SELECT * FROM csq_f(csq_r.a));

SELECT * FROM csq_r WHERE exists (SELECT * FROM csq_f(csq_r.a));

-- force_explain
explain SELECT * FROM csq_r WHERE not exists (SELECT * FROM csq_f(csq_r.a));

SELECT * FROM csq_r WHERE not exists (SELECT * FROM csq_f(csq_r.a));

-- force_explain
explain SELECT * FROM csq_r WHERE a > (SELECT csq_f FROM csq_f(csq_r.a) limit 1);

SELECT * FROM csq_r WHERE a > (SELECT csq_f FROM csq_f(csq_r.a) limit 1);

-- force_explain
explain SELECT * FROM csq_r WHERE a < ANY (SELECT csq_f FROM csq_f(csq_r.a));

SELECT * FROM csq_r WHERE a < ANY (SELECT csq_f FROM csq_f(csq_r.a));

-- force_explain
explain SELECT * FROM csq_r WHERE a <= ALL (SELECT csq_f FROM csq_f(csq_r.a));

SELECT * FROM csq_r WHERE a <= ALL (SELECT csq_f FROM csq_f(csq_r.a));

-- force_explain
explain SELECT * FROM csq_r WHERE a IN (SELECT csq_f FROM csq_f(csq_r.a),csq_r);

SELECT * FROM csq_r WHERE a IN (SELECT csq_f FROM csq_f(csq_r.a),csq_r);

--
-- Test pullup of expr CSQs to joins
--

--
-- Test data
--

drop table if exists csq_pullup;
create table csq_pullup(t text, n numeric, i int, v varchar(10)) distributed by (t);
insert into csq_pullup values ('abc',1, 2, 'xyz');
insert into csq_pullup values ('xyz',2, 3, 'def');  
insert into csq_pullup values ('def',3, 1, 'abc'); 

--
-- Expr CSQs to joins
--

--
-- text, text
--

explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.t);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.t);

--
-- text, varchar
--

explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.v);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.v);

--
-- numeric, numeric
--

explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n=t1.n);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n=t1.n);

--
-- function(numeric), function(numeric)
--

explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n + 1=t1.n + 1);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n + 1=t1.n + 1);


--
-- function(numeric), function(int)
--

explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n + 1=t1.i + 1);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n + 1=t1.i + 1);


--
-- NOT EXISTS CSQs to joins
--

--
-- text, text
--

explain select * from csq_pullup t0 where not exists (select 1 from csq_pullup t1 where t0.t=t1.t and t1.i = 1);

select * from csq_pullup t0 where not exists (select 1 from csq_pullup t1 where t0.t=t1.t and t1.i = 1);

--
-- int, function(int)
--

explain select * from csq_pullup t0 where not exists (select 1 from csq_pullup t1 where t0.i=t1.i + 1);

select * from csq_pullup t0 where not exists (select 1 from csq_pullup t1 where t0.i=t1.i + 1);

--
-- wrong results bug MPP-16477
--

drop table if exists subselect_t1;
drop table if exists subselect_t2;

create table subselect_t1(x int) distributed by (x);
insert into subselect_t1 values(1),(2);

create table subselect_t2(y int) distributed by (y);
insert into subselect_t2 values(1),(2),(2);

analyze subselect_t1;
analyze subselect_t2;

explain select * from subselect_t1 where x in (select y from subselect_t2);

select * from subselect_t1 where x in (select y from subselect_t2);

-- start_ignore
-- Known_opt_diff: MPP-21351
-- end_ignore
explain select * from subselect_t1 where x in (select y from subselect_t2 union all select y from subselect_t2);

select * from subselect_t1 where x in (select y from subselect_t2 union all select y from subselect_t2);

explain select count(*) from subselect_t1 where x in (select y from subselect_t2);

select count(*) from subselect_t1 where x in (select y from subselect_t2);

-- start_ignore
-- Known_opt_diff: MPP-21351
-- end_ignore
explain select count(*) from subselect_t1 where x in (select y from subselect_t2 union all select y from subselect_t2);

select count(*) from subselect_t1 where x in (select y from subselect_t2 union all select y from subselect_t2);

select count(*) from 
       ( select 1 as FIELD_1 union all select 2 as FIELD_1 ) TABLE_1 
       where FIELD_1 in ( select 1 as FIELD_1 union all select 1 as FIELD_1 union all select 1 as FIELD_1 );
       
--
-- Query was deadlocking because of not squelching subplans (MPP-18936)
--
drop table if exists t1; 
drop table if exists t2; 
drop table if exists t3; 
drop table if exists t4; 

CREATE TABLE t1 AS (SELECT generate_series(1, 5000) AS i, generate_series(5001, 10000) AS j);
CREATE TABLE t2 AS (SELECT * FROM t1 WHERE gp_segment_id = 0);
CREATE TABLE t3 AS (SELECT * FROM t1 WHERE gp_segment_id = 1);
CREATE TABLE t4 (i1 int, i2 int); 

set gp_interconnect_queue_depth=1;

-- This query was deadlocking on a 2P system
INSERT INTO t4 
(
SELECT t1.i, (SELECT t3.i FROM t3 WHERE t3.i + 1 = t1.i + 1)
FROM t1, t3
WHERE t1.i = t3.i
)
UNION
(
SELECT t1.i, (SELECT t2.i FROM t2 WHERE t2.i + 1 = t1.i + 1)
FROM t1, t2
WHERE t1.i = t2.i
);

drop table if exists t1; 
drop table if exists t2; 
drop table if exists t3; 
drop table if exists t4; 

--
-- Initplans with no corresponding params should be removed MPP-20600
--

drop table if exists t1;
drop table if exists t2;

create table t1(a int);
create table t2(b int);

select * from t1 where a=1 and a=2 and a > (select t2.b from t2);

explain select * from t1 where a=1 and a=2 and a > (select t2.b from t2);

explain select * from t1 where a=1 and a=2 and a > (select t2.b from t2)
union all
select * from t1 where a=1 and a=2 and a > (select t2.b from t2);

select * from t1 where a=1 and a=2 and a > (select t2.b from t2)
union all
select * from t1 where a=1 and a=2 and a > (select t2.b from t2);

explain select * from t1,
(select * from t1 where a=1 and a=2 and a > (select t2.b from t2)) foo
where t1.a = foo.a;

select * from t1,
(select * from t1 where a=1 and a=2 and a > (select t2.b from t2)) foo
where t1.a = foo.a;

--
-- Correlated subqueries with limit/offset clause must not be pulled up as join
--
insert into t1 values (1);
insert into t2 values (1);
explain select 1 from t1 where a in (select b from t2 where a = 1 limit 1);
explain select 1 from t1 where a in (select b from t2 where a = 1 offset 1);
select 1 from t1 where a in (select b from t2 where a = 1 limit 1);
select 1 from t1 where a in (select b from t2 where a = 1 offset 1);

drop table if exists t1;
drop table if exists t2;

--
-- Test for a bug we used to have with eliminating InitPlans. The subplan,
-- (select max(content) from y), was eliminated when it shouldn't have been.
-- The query is supposed to return 0 rows, but returned > 0 when the bug was
-- present.
--
CREATE TABLE initplan_x (i int4, t text);
insert into initplan_x values
 (1, 'foobar1'),
 (2, 'foobar2'),
 (3, 'foobar3'),
 (4, 'foobar4'),
 (5, 'foobar5');

CREATE TABLE initplan_y (content int4);
insert into initplan_y values (5);

select i, t from initplan_x
except
select g, t from initplan_x,
                 generate_series(0, (select max(content) from initplan_y)) g
order by 1;

drop table if exists initplan_x;
drop table if exists initplan_y;

--
-- Test Initplans that return multiple params.
--
create table initplan_test(i int, j int, m int);
insert into initplan_test values (1,1,1);
select * from initplan_test where row(j, m) = (select j, m from initplan_test where i = 1);

drop table initplan_test;

--
-- apply parallelization for subplan MPP-24563
--
create table t1_mpp_24563 (id int, value int) distributed by (id);
insert into t1_mpp_24563 values (1, 3);

create table t2_mpp_24563 (id int, value int, seq int) distributed by (id);
insert into t2_mpp_24563 values (1, 7, 5);

explain select row_number() over (order by seq asc) as id, foo.cnt
from
(select seq, (select count(*) from t1_mpp_24563 t1 where t1.id = t2.id) cnt from
	t2_mpp_24563 t2 where value = 7) foo;

drop table t1_mpp_24563;
drop table t2_mpp_24563;

--
-- MPP-20470 update the flow of node after parallelizing subplan.
--
CREATE TABLE t_mpp_20470 (
    col_date timestamp without time zone,
    col_name character varying(6),
    col_expiry date
) DISTRIBUTED BY (col_date) PARTITION BY RANGE(col_date)
(
START ('2013-05-10 00:00:00'::timestamp without time zone) END ('2013-05-11
	00:00:00'::timestamp without time zone) WITH (tablename='t_mpp_20470_ptr1', appendonly=false ),
START ('2013-05-24 00:00:00'::timestamp without time zone) END ('2013-05-25
	00:00:00'::timestamp without time zone) WITH (tablename='t_mpp_20470_ptr2', appendonly=false )
);

COPY t_mpp_20470 from STDIN delimiter '|' null '';
2013-05-10 00:00:00|OPTCUR|2013-05-29
2013-05-10 04:35:20|OPTCUR|2013-05-29
2013-05-24 03:10:30|FUTCUR|2014-04-28
2013-05-24 05:32:34|OPTCUR|2013-05-29
\.

create view v1_mpp_20470 as
SELECT
CASE
	WHEN  b.col_name::text = 'FUTCUR'::text
	THEN  ( SELECT count(a.col_expiry) AS count FROM t_mpp_20470 a WHERE
		a.col_name::text = b.col_name::text)::text
	ELSE 'Q2'::text END  AS  cc,  1 AS nn
FROM t_mpp_20470 b;

explain SELECT  cc, sum(nn) over() FROM v1_mpp_20470;

drop view v1_mpp_20470;
drop table t_mpp_20470;

create table tbl_25484(id int, num int) distributed by (id);
insert into tbl_25484 values(1, 1), (2, 2), (3, 3);
select id from tbl_25484 where 3 = (select 3 where 3 = (select num));
drop table tbl_25484;
reset optimizer_segments;
reset optimizer_nestloop_factor;

--
-- Test case that once triggered a bug in the IN-clause pull-up code.
--
SELECT p.id
    FROM (SELECT * FROM generate_series(1,10) id
          WHERE id IN (
              SELECT 1
              UNION ALL
              SELECT 0)) p;

--
-- Verify another bug in the IN-clause pull-up code. This returned some
-- rows from xsupplier twice, because of a bug in detecting whether a
-- Redistribute node was needed.
--
CREATE TABLE xlineitem (l_orderkey int4, l_suppkey int4) distributed by (l_orderkey);
insert into xlineitem select g+3, g from generate_series(10,100) g;
insert into xlineitem select g+1, g from generate_series(10,100) g;
insert into xlineitem select g, g from generate_series(10,100) g;

CREATE TABLE xsupplier (s_suppkey int4, s_name text) distributed by (s_suppkey);
insert into xsupplier select g, 'foo' || g from generate_series(1,10) g;

select s_name from xsupplier
where s_suppkey in (
  select g.l_suppkey from xlineitem g
) ;

--
-- Another case that failed at one point. (A planner bug in pulling up a
-- subquery with constant distribution key, 1, in the outer queries.)
--
create table nested_in_tbl(tc1 int, tc2 int) distributed by (tc1);
select * from nested_in_tbl t1  where tc1 in
  (select 1 from nested_in_tbl t2 where tc1 in
    (select 1 from nested_in_tbl t3 where t3.tc2 = t2.tc2));
drop table nested_in_tbl;

--
-- Window query with a function scan that has non-correlated subquery.
--
SELECT rank() over (partition by min(c) order by min(c)) AS p_rank FROM (SELECT d AS c FROM (values(1)) d1, generate_series(0,(SELECT 2)) AS d) tt GROUP BY c;

--
-- Remove unused subplans
--
create table foo(a int, b int) distributed by (a) partition by range(b) (start(1) end(3) every(1));
create table bar(a int, b int) distributed by (a);

with CT as (select a from foo except select a from bar)
select * from foo
where exists (select 1 from CT where CT.a = foo.a);
--
-- Multiple SUBPLAN nodes must not refer to same plan_id
--
CREATE TABLE bar_s (c integer, d character varying(10));
INSERT INTO bar_s VALUES (9,9);
SELECT * FROM bar_s T1 WHERE c = (SELECT max(c) FROM bar_s T2 WHERE T2.d = T1.d GROUP BY c) AND c < 10;
CREATE TABLE foo_s (a integer, b integer)  PARTITION BY RANGE(b)
    (PARTITION sub_one START (1) END (10),
     PARTITION sub_two START (11) END (22));
INSERT INTO foo_s VALUES (9,9);
INSERT INTO foo_s VALUES (2,9);
SELECT bar_s.c from bar_s, foo_s WHERE foo_s.a=2 AND foo_s.b = (SELECT max(b) FROM foo_s WHERE bar_s.c = 9);
CREATE TABLE baz_s (i int4);
INSERT INTO baz_s VALUES (9);
SELECT bar_s.c FROM bar_s, foo_s WHERE foo_s.b = (SELECT max(i) FROM baz_s WHERE bar_s.c = 9) AND foo_s.b = bar_s.d::int4;
DROP TABLE bar_s;
DROP TABLE foo_s;
DROP TABLE baz_s;

--
-- EXPLAIN tests for queries in subselect.sql to significant plan changes
--

-- Set up some simple test tables

CREATE TABLE SUBSELECT_TBL (
  f1 integer,
  f2 integer,
  f3 float
);

INSERT INTO SUBSELECT_TBL VALUES (1, 2, 3);
INSERT INTO SUBSELECT_TBL VALUES (2, 3, 4);
INSERT INTO SUBSELECT_TBL VALUES (3, 4, 5);
INSERT INTO SUBSELECT_TBL VALUES (1, 1, 1);
INSERT INTO SUBSELECT_TBL VALUES (2, 2, 2);
INSERT INTO SUBSELECT_TBL VALUES (3, 3, 3);
INSERT INTO SUBSELECT_TBL VALUES (6, 7, 8);
INSERT INTO SUBSELECT_TBL VALUES (8, 9, NULL);

ANALYZE SUBSELECT_TBL;

-- Uncorrelated subselects

EXPLAIN SELECT '' AS six, f1 AS "Uncorrelated Field" FROM SUBSELECT_TBL
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL) ORDER BY 2;

EXPLAIN SELECT '' AS six, f1 AS "Uncorrelated Field" FROM SUBSELECT_TBL
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE
    f2 IN (SELECT f1 FROM SUBSELECT_TBL)) ORDER BY 2;

EXPLAIN SELECT '' AS three, f1, f2
  FROM SUBSELECT_TBL
  WHERE (f1, f2) NOT IN (SELECT f2, CAST(f3 AS int4) FROM SUBSELECT_TBL
                         WHERE f3 IS NOT NULL) ORDER BY 2,3;

ANALYZE tenk1;
EXPLAIN SELECT * FROM tenk1 a, tenk1 b
WHERE (a.unique1,b.unique2) IN (SELECT unique1,unique2 FROM tenk1 c);

-- Correlated subselects

EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1) ORDER BY 2,3;

EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f3 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f1 IN
    (SELECT f2 FROM SUBSELECT_TBL WHERE CAST(upper.f2 AS float) = f3) ORDER BY 2,3;

EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f3 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f3 IN (SELECT upper.f1 + f2 FROM SUBSELECT_TBL
               WHERE f2 = CAST(f3 AS integer)) ORDER BY 2,3;

EXPLAIN SELECT '' AS five, f1 AS "Correlated Field"
  FROM SUBSELECT_TBL
  WHERE (f1, f2) IN (SELECT f2, CAST(f3 AS int4) FROM SUBSELECT_TBL
                     WHERE f3 IS NOT NULL) ORDER BY 2;

--
-- Test cases to catch unpleasant interactions between IN-join processing
-- and subquery pullup.
--

EXPLAIN select count(*) from
  (select 1 from tenk1 a
   where unique1 IN (select hundred from tenk1 b)) ss;
EXPLAIN select count(distinct ss.ten) from
  (select ten from tenk1 a
   where unique1 IN (select hundred from tenk1 b)) ss;
EXPLAIN select count(*) from
  (select 1 from tenk1 a
   where unique1 IN (select distinct hundred from tenk1 b)) ss;
EXPLAIN select count(distinct ss.ten) from
  (select ten from tenk1 a
   where unique1 IN (select distinct hundred from tenk1 b)) ss;

--
-- In case of simple exists query, planner can generate alternative
-- subplans and choose one of them during execution based on the cost.
-- The below test check that we are generating alternative subplans,
-- we should see 2 subplans in the explain
--
EXPLAIN SELECT EXISTS(SELECT * FROM tenk1 WHERE tenk1.unique1 = tenk2.unique1) FROM tenk2 LIMIT 1;

SELECT EXISTS(SELECT * FROM tenk1 WHERE tenk1.unique1 = tenk2.unique1) FROM tenk2 LIMIT 1;

--
-- Ensure that NOT is not lost during subquery pull-up
--
SELECT 1 AS col1 WHERE NOT (SELECT 1 = 1);

--
-- Test sane behavior in case of semi join semantics
--
-- start_ignore
DROP TABLE IF EXISTS dedup_test1;
DROP TABLE IF EXISTS dedup_test2;
DROP TABLE IF EXISTS dedup_test3;
-- end_ignore
CREATE TABLE dedup_test1 ( a int, b int ) DISTRIBUTED BY (a);
CREATE TABLE dedup_test2 ( e int, f int ) DISTRIBUTED BY (e);
CREATE TABLE dedup_test3 ( a int, b int, c int) DISTRIBUTED BY (a) PARTITION BY RANGE(c) (START(1) END(2) EVERY(1)); 

INSERT INTO dedup_test1 select i, i from generate_series(1,4)i;
INSERT INTO dedup_test2 select i, i from generate_series(1,4)i;
INSERT INTO dedup_test3 select 1, 1, 1 from generate_series(1,10);
ANALYZE dedup_test1;
ANALYZE dedup_test2;
ANALYZE dedup_test3;

EXPLAIN SELECT * FROM dedup_test1 INNER JOIN dedup_test2 ON dedup_test1.a= dedup_test2.e WHERE (a) IN (SELECT a FROM dedup_test3);
SELECT * FROM dedup_test1 INNER JOIN dedup_test2 ON dedup_test1.a= dedup_test2.e WHERE (a) IN (SELECT a FROM dedup_test3);

-- Test planner to check if it optimizes the join and marks it as a dummy join
EXPLAIN SELECT * FROM dedup_test3, dedup_test1 WHERE c = 7 AND dedup_test3.b IN (SELECT b FROM dedup_test1);
EXPLAIN SELECT * FROM dedup_test3, dedup_test1 WHERE c = 7 AND dedup_test3.b IN (SELECT a FROM dedup_test1);
EXPLAIN SELECT * FROM dedup_test3, dedup_test1 WHERE c = 7 AND EXISTS (SELECT b FROM dedup_test1) AND dedup_test3.b IN (SELECT b FROM dedup_test1);

-- start_ignore
DROP TABLE IF EXISTS dedup_test1;
DROP TABLE IF EXISTS dedup_test2;
DROP TABLE IF EXISTS dedup_test3;
-- end_ignore

-- Test init/main plan are not both parallel
create table init_main_plan_parallel (c1 int, c2 int);
-- case 1: init plan is parallel, main plan is not.
select relname from pg_class where exists(select * from init_main_plan_parallel);
-- case2: init plan is not parallel, main plan is parallel
select * from init_main_plan_parallel where exists (select * from pg_class);


-- A subplan whose targetlist might be expanded to make sure all entries of its
-- hashExpr are in its targetlist, test the motion node above it also updated
-- its targetlist, otherwise, a wrong answer or a crash happens.
DROP TABLE IF EXISTS TEST_IN;
CREATE TABLE TEST_IN(
    C01  FLOAT,
    C02  NUMERIC(10,0)
) DISTRIBUTED RANDOMLY;

--insert repeatable records:
INSERT INTO TEST_IN
SELECT
    ROUND(RANDOM()*1E1),ROUND(RANDOM()*1E1)
FROM GENERATE_SERIES(1,1E4::BIGINT) I;

ANALYZE TEST_IN;

SELECT COUNT(*) FROM
TEST_IN A
WHERE A.C01 IN(SELECT C02 FROM TEST_IN);
