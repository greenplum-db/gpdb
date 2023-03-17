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
analyze mrs_t1;

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
analyze csq_m1;

drop table if exists csq_d1;
create table csq_d1(x int) distributed by (x);
insert into csq_d1 select * from csq_m1;
analyze csq_d1;

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
analyze csq_m1;

drop table if exists csq_d1;
create table csq_d1(x int) distributed by (x);
insert into csq_d1 select * from csq_m1 where x < 3;
insert into csq_d1 values(4);
analyze csq_d1;

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
analyze csq_pullup;
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
-- Test a few cases where pulling up an aggregate subquery is not possible
--

-- subquery contains a LIMIT
explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.t LIMIT 1);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.t LIMIT 1);

-- subquery contains a HAVING clause
explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.t HAVING count(*) < 10);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.t=t1.t HAVING count(*) < 10);

-- subquery contains quals of form 'function(outervar, innervar1) = innvervar2'
explain select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n + t1.n =t1.i);

select * from csq_pullup t0 where 1= (select count(*) from csq_pullup t1 where t0.n + t1.n =t1.i);


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

drop table foo;
drop table bar;

--
-- Multiple SUBPLAN nodes referring to the same plan_id
--
CREATE TABLE bar_s (c integer, d character varying(10));
INSERT INTO bar_s VALUES (9,9);
ANALYZE bar_s;
SELECT * FROM bar_s T1 WHERE c = (SELECT max(c) FROM bar_s T2 WHERE T2.d = T1.d GROUP BY c) AND c < 10;
CREATE TABLE foo_s (a integer, b integer)  PARTITION BY RANGE(b)
    (PARTITION sub_one START (1) END (10),
     PARTITION sub_two START (11) END (22));
INSERT INTO foo_s VALUES (9,9);
INSERT INTO foo_s VALUES (2,9);
SELECT bar_s.c from bar_s, foo_s WHERE foo_s.a=2 AND foo_s.b = (SELECT max(b) FROM foo_s WHERE bar_s.c = 9);
CREATE TABLE baz_s (i int4);
INSERT INTO baz_s VALUES (9);
ANALYZE baz_s;

-- In this query, the planner avoids using SubPlan 1 in the qual in the join,
-- because it avoids picking SubPlans from an equivalence class, when it has
-- other choices.
SELECT bar_s.c FROM bar_s, foo_s WHERE foo_s.b = (SELECT max(i) FROM baz_s WHERE bar_s.c = 9) AND foo_s.b = bar_s.d::int4;

-- Same as above, but with another subquery, so it must use a SubPlan. There
-- are two references to the same SubPlan in the plan, on different slices.
-- GPDB_96_MERGE_FIXME: this EXPLAIN output should become nicer-looking once we
-- merge upstream commit 4d042999f9, to suppress the SubPlans from being
-- printed twice.
explain SELECT bar_s.c FROM bar_s, foo_s WHERE foo_s.b = (SELECT max(i) FROM baz_s WHERE bar_s.c = 9) AND foo_s.b = (select bar_s.d::int4);
SELECT bar_s.c FROM bar_s, foo_s WHERE foo_s.b = (SELECT max(i) FROM baz_s WHERE bar_s.c = 9) AND foo_s.b = (select bar_s.d::int4);

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

-- Test simplify group-by/order-by inside subquery if sublink pull-up is possible
EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1 GROUP BY f2);

EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1  GROUP BY f2 LIMIT 3);

EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1 ORDER BY f2);

EXPLAIN SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"
  FROM SUBSELECT_TBL upper
  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1  ORDER BY f2 LIMIT 3);


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


-- More dedup semi-join tests.
create table dedup_tab (a int4) distributed by(a) ;
insert into dedup_tab select g from  generate_series(1,100) g;
analyze dedup_tab;

create table dedup_reptab (a int4) distributed replicated;
insert into dedup_reptab select generate_series(1,1);
analyze dedup_reptab;

-- Replicated table on the inner side of the join. The replicated table needs
-- be broadcast from a single node to the others, with a unique RowIdExpr
-- tacked on, because even though all the rows are available in all the
-- segments, you cannot distinguish join rows generated by the same "logical"
-- row otherwise.
explain (costs off)
select * from dedup_reptab r where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_reptab r where r.a in (select t.a/10 from dedup_tab t);

-- Try the same with a General-locus function. In GPDB 6 and below, this
-- generated a plan that did create the same logical row ID on each segment,
-- on the assumption that an immutable function generates the result rows
-- in the same order on all segments. We no longer assume that, and generate
-- the same plan with a broadcast as the case with a replicated table.
--
-- We have to create a custom function for this, instead of using
-- generate_series() directly, because the rows-estimate for generate_series()
-- is so high that we don't get the plan we want. (After PostgreSQL v12 we
-- could though, because the cost estimation of functions was improved.)
create function dedup_srf() RETURNS SETOF int AS $$
  begin
    return query select generate_series(1, 3);
  end;
$$ LANGUAGE plpgsql IMMUTABLE ROWS 3;

create function dedup_srf_stable() RETURNS SETOF int AS $$
  begin
    return query select generate_series(1, 3);
  end;
$$ LANGUAGE plpgsql STABLE ROWS 3;

create function dedup_srf_volatile() RETURNS SETOF int AS $$
  begin
    return query select generate_series(1, 3);
  end;
$$ LANGUAGE plpgsql VOLATILE ROWS 3;

explain (costs off)
select * from dedup_srf() r(a) where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_srf() r(a) where r.a in (select t.a/10 from dedup_tab t);

explain (costs off)
select * from dedup_srf_stable() r(a) where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_srf_stable() r(a) where r.a in (select t.a/10 from dedup_tab t);

explain (costs off)
select * from dedup_srf_volatile() r(a) where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_srf_volatile() r(a) where r.a in (select t.a/10 from dedup_tab t);

-- Also test it with non-SRFs. In principle, since the function returns exactly
-- one row, no deduplication would be needed in these cases. But the planner
-- doesn't recognize that currently, so you get the same kind of plan as with
-- set-returning functions.
create function dedup_func() RETURNS int AS $$
  select 5;
$$ LANGUAGE SQL IMMUTABLE;
create function dedup_func_stable() RETURNS int AS $$
  select 5;
$$ LANGUAGE SQL STABLE;
create function dedup_func_volatile() RETURNS int AS $$
  select 5;
$$ LANGUAGE SQL VOLATILE;

explain (costs off)
select * from dedup_func() r(a) where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_func() r(a) where r.a in (select t.a/10 from dedup_tab t);

explain (costs off)
select * from dedup_func_stable() r(a) where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_func_stable() r(a) where r.a in (select t.a/10 from dedup_tab t);

explain (costs off)
select * from dedup_func_volatile() r(a) where r.a in (select t.a/10 from dedup_tab t);
select * from dedup_func_volatile() r(a) where r.a in (select t.a/10 from dedup_tab t);


--
-- Test init/main plan are not both parallel
--
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

--
-- Variant of the test in upstream 'subselect' test, for PostgreSQL bug #14924
-- At one point, this produced wrong results on GPDB for different reasons than
-- the original bug: we forgot to handle the VALUES list in the function to
-- mutate a plan tree (plan_tree_mutator()).
--
create temp table onerowtmp as select 1;
select val.x
  from generate_series(1,10) as s(i),
  lateral (
    values ((select s.i + 1 from onerowtmp)), (s.i + 101)
  ) as val(x)
where s.i < 10 and val.x < 110;

-- EXISTS sublink simplication

drop table if exists simplify_sub;

create table simplify_sub (i int) distributed by (i);
insert into simplify_sub values (1);
insert into simplify_sub values (2);
analyze simplify_sub;

-- limit n
explain (costs off)
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 1);
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 1);

explain (costs off)
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 1);
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 1);

explain (costs off)
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 0);
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 0);

explain (costs off)
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 0);
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit 0);

explain (costs off)
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit all);
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit all);

explain (costs off)
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit all);
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit all);

explain (costs off)
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit NULL);
select * from simplify_sub t1 where exists (select 1 from simplify_sub t2 where t1.i = t2.i limit NULL);

explain (costs off)
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit NULL);
select * from simplify_sub t1 where not exists (select 1 from simplify_sub t2 where t1.i = t2.i limit NULL);

-- aggregates without GROUP BY or HAVING
explain (costs off)
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i);
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i);

explain (costs off)
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i);
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i);

explain (costs off)
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 0);
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 0);

explain (costs off)
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 0);
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 0);

explain (costs off)
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 1);
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 1);

explain (costs off)
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 1);
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset 1);

explain (costs off)
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset NULL);
select * from simplify_sub t1 where exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset NULL);

explain (costs off)
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset NULL);
select * from simplify_sub t1 where not exists (select sum(t2.i) from simplify_sub t2 where t1.i = t2.i offset NULL);

drop table if exists simplify_sub;

--
-- Test a couple of cases where a SubPlan is used in a Motion's hash key.
--
create table foo (i int4, j int4) distributed by (i);
create table bar (i int4, j int4) distributed by (i);
create table baz (i int4, j int4) distributed by (i);
insert into foo select g, g from generate_series(1, 10) g;
insert into bar values (1, 1);
insert into baz select g, g from generate_series(5, 100) g;
analyze foo;
analyze bar;
analyze baz;

explain (verbose, costs off)
select * from foo left outer join baz on (select bar.i from bar where bar.i = foo.i) + 1  = baz.j;
select * from foo left outer join baz on (select bar.i from bar where bar.i = foo.i) + 1  = baz.j;

-- This is a variant of a query in the upstream 'subselect' test, with the
-- twist that baz.i is the distribution key for the table. In the plan, the
-- CASE WHEN construct with SubPlan is used as Hash Key in the Redistribute
-- Motion. It is a planned as a hashed SubPlan. (We had a bug at one point,
-- where the hashed SubPlan was added to the target list twice, which
-- caused an error at runtime when the executor tried to build the hash
-- table twice, because the Motion in the SubPlan couldn't be rescanned.)
explain (verbose, costs off)
select * from foo where
  (case when foo.i in (select a.i from baz a) then foo.i else null end) in
  (select b.i from baz b);
select * from foo where
  (case when foo.i in (select a.i from baz a) then foo.i else null end) in
  (select b.i from baz b);

-- When creating plan with subquery and CTE, it sets the useless flow for the plan.
-- But we only need flow for the topmost plan and child of the motion. See commit
-- https://github.com/greenplum-db/gpdb/commit/93abe741cd67f04958e2951edff02b45ab6e280f for detail
-- The extra flow will cause subplan set wrong motionType and cause an ERROR
-- unexpected gang size: XX
-- This related to issue: https://github.com/greenplum-db/gpdb/issues/12371
create table extra_flow_dist(a int, b int, c date);
create table extra_flow_dist1(a int, b int);

insert into extra_flow_dist select i, i, '1949-10-01'::date from generate_series(1, 10)i;
insert into extra_flow_dist1 select i, i from generate_series(20, 22)i;

-- case 1 subplan with outer general locus (CTE and subquery)
explain (verbose, costs off) with run_dt as (
	select
	(
	  select c from extra_flow_dist where b = x  -- subplan's outer is the below general locus path
	) dt
	from (select ( max(1) ) x) a  -- general locus
)
select * from run_dt, extra_flow_dist1
where dt < '2010-01-01'::date;

with run_dt as (
	select
	(
	  select c from extra_flow_dist where b = x
	) dt
	from (select ( max(1) ) x) a
)
select * from run_dt, extra_flow_dist1
where dt < '2010-01-01'::date;

create table extra_flow_rand(a int) distributed replicated;
insert into extra_flow_rand values (1);

-- case 2 for subplan with outer segment general locus (CTE and subquery)
explain (verbose, costs off) with run_dt as (
	select
	(
		select c from extra_flow_dist where b = x  -- subplan's outer is the below segment general locus path
	) dt
	from (select a x from extra_flow_rand) a  -- segment general locus
)
select * from run_dt, extra_flow_dist1
where dt < '2010-01-01'::date;

with run_dt as (
	select
	(
		select c from extra_flow_dist where b = x
	) dt
	from (select a x from extra_flow_rand) a
)
select * from run_dt, extra_flow_dist1
where dt < '2010-01-01'::date;

-- case 3 for subplan with outer entry locus (CTE and subquery)
explain (verbose, costs off) with run_dt as (
	select
	(
		select c from extra_flow_dist where b = x  -- subplan's outer is the below entry locus path
	) dt
	from (select 1 x from pg_class limit 1) a  -- entry locus
)
select * from run_dt, extra_flow_dist1
where dt < '2010-01-01'::date;

with run_dt as (
	select
	(
		select c from extra_flow_dist where b = x  -- subplan's outer is the below entry locus path
	) dt
	from (select 1 x from pg_class limit 1) a  -- entry locus
)
select * from run_dt, extra_flow_dist1
where dt < '2010-01-01'::date;

-- case 4 subplan with outer segment general locus without param in subplan (CTE and subquery)
explain (verbose, costs off) with run_dt as (
	select x, y dt
	from (select a x from extra_flow_rand ) a  -- segment general locus
	left join (select max(1) y) aaa
	on a.x > any (select random() from extra_flow_dist)  -- subplan's outer is the above segment general locus path
)
select * from run_dt, extra_flow_dist1
where dt < extra_flow_dist1.a;

-- case 5 for subplan with outer entry locus without param in subplan (CTE and subquery)
explain (verbose, costs off) with run_dt as (
	select x, y dt
	from (select relnatts x from pg_class ) a  -- entry locus
	left join (select max(1) y) aaa
	on a.x > any (select random() from extra_flow_dist)  -- subplan's outer is the above entry loucs
)
select * from run_dt, extra_flow_dist1
where dt < extra_flow_dist1.a;

-- case 6 without CTE, nested subquery should not add extral flow
explain (verbose, costs off) select * from (
	select dt from (
		select
		(
			select c from extra_flow_dist where b = x  -- subplan's outer is the below general locus path
		) dt
		from (select ( max(1) ) x) a  -- general locus
		union
		select
		(
			select c from extra_flow_dist where b = x  -- subplan's outer is the below general locus path
		) dt
		from (select ( max(1) ) x) aa  -- general locus
	) tbl
) run_dt,
extra_flow_dist1
where dt < '2010-01-01'::date;

-- Check DISTINCT ON clause and ORDER BY clause in SubLink, See https://github.com/greenplum-db/gpdb/issues/12656.
-- For EXISTS SubLink, we don’t need to care about the data deduplication problem, we can delete DISTINCT ON clause and
-- ORDER BY clause with confidence, because we only care about whether the data exists.
-- But for ANY SubLink, wo can't do this, because we not only care about the existence of data, but also the content of
-- the data.
create table issue_12656 (
    i int,
    j int
) distributed by (i);

insert into issue_12656 values (1, 10001), (1, 10002);

-- case 1, check basic DISTINCT ON
explain (costs off, verbose)
select * from issue_12656 where (i, j) in (select distinct on (i) i, j from issue_12656);

select * from issue_12656 where (i, j) in (select distinct on (i) i, j from issue_12656);

-- case 2, check DISTINCT ON and ORDER BY
explain (costs off, verbose)
select * from issue_12656 where (i, j) in (select distinct on (i) i, j from issue_12656 order by i, j asc);

select * from issue_12656 where (i, j) in (select distinct on (i) i, j from issue_12656 order by i, j asc);

explain (costs off, verbose)
select * from issue_12656 where (i, j) in (select distinct on (i) i, j from issue_12656 order by i, j desc);

select * from issue_12656 where (i, j) in (select distinct on (i) i, j from issue_12656 order by i, j desc);

-- case 3, check correlated DISTINCT ON
explain select * from issue_12656 a where (i, j) in
(select distinct on (i) i, j from issue_12656 b where a.i=b.i order by i, j asc);

select * from issue_12656 a where (i, j) in
(select distinct on (i) i, j from issue_12656 b where a.i=b.i order by i, j asc);

---
--- Test param info is preserved when bringing a path to OuterQuery locus
---
drop table if exists param_t;

create table param_t (i int, j int);
insert into param_t select i, i from generate_series(1,10)i;
analyze param_t;

explain (costs off)
select * from param_t a where a.i in
	(select count(b.j) from param_t b, param_t c,
		lateral (select * from param_t d where d.j = c.j limit 10) s
	where s.i = a.i
	);
select * from param_t a where a.i in
	(select count(b.j) from param_t b, param_t c,
		lateral (select * from param_t d where d.j = c.j limit 10) s
	where s.i = a.i
	);


drop table if exists param_t;

-- A guard test case for gpexpand's populate SQL
-- Some simple notes and background is: we want to compute
-- table size efficiently, it is better to avoid invoke
-- pg_relation_size() in serial on QD, since this function
-- will dispatch for each tuple. The bad pattern SQL is like
--   select pg_relation_size(oid) from pg_class where xxx
-- The idea is force pg_relations_size is evaluated on each
-- segment and the sum the result together to get the final
-- result. To make sure correctness, we have to evaluate
-- pg_relation_size before any motion. The skill here is
-- to wrap this in a subquery, due to volatile of pg_relation_size,
-- this subquery won't be pulled up. Plus the skill of
-- gp_dist_random('pg_class') we can achieve this goal.

-- the below test is to verify the plan, we should see pg_relation_size
-- is evaludated on each segment and then motion then sum together. The
-- SQL pattern is a catalog join a table size "dict".

set gp_enable_multiphase_agg = on;
-- force nestloop join to make test stable since we
-- are testing plan and do not care about where we
-- put hash table.
set enable_hashjoin = off;
set enable_nestloop = on;
set enable_indexscan = off;
set enable_bitmapscan = off;

explain (verbose on, costs off)
with cte(table_oid, size) as
(
   select
     table_oid,
     sum(size) size
   from (
     select oid,
          pg_relation_size(oid)
     from gp_dist_random('pg_class')
   ) x(table_oid, size)
  group by table_oid
)
select pc.relname, ts.size
from pg_class pc, cte ts
where pc.oid = ts.table_oid;

set gp_enable_multiphase_agg = off;

explain (verbose on, costs off)
with cte(table_oid, size) as
(
   select
     table_oid,
     sum(size) size
   from (
     select oid,
          pg_relation_size(oid)
     from gp_dist_random('pg_class')
   ) x(table_oid, size)
  group by table_oid
)
select pc.relname, ts.size
from pg_class pc, cte ts
where pc.oid = ts.table_oid;

reset gp_enable_multiphase_agg;
reset enable_hashjoin;
reset enable_nestloop;
reset enable_indexscan;
reset enable_bitmapscan;
create table sublink_outer_table(a int, b int) distributed by(b);
create table sublink_inner_table(x int, y bigint) distributed by(y);

set optimizer to off;
explain select t.* from sublink_outer_table t join (select y ,10*avg(x) s from sublink_inner_table group by y) RR on RR.y = t.b and t.a > rr.s;
explain select * from sublink_outer_table T where a > (select 10*avg(x) from sublink_inner_table R where T.b=R.y);

set enable_hashagg to off;
explain select t.* from sublink_outer_table t join (select y ,10*avg(x) s from sublink_inner_table group by y) RR on RR.y = t.b and t.a > rr.s;
explain select * from sublink_outer_table T where a > (select 10*avg(x) from sublink_inner_table R where T.b=R.y);
drop table sublink_outer_table;
drop table sublink_inner_table;
reset optimizer;
reset enable_hashagg;

-- Ensure sub-queries with order by outer reference can be decorrelated and executed correctly.
create table r(a int, b int, c int) distributed by (a);
create table s(a int, b int, c int) distributed by (a);
insert into r values (1,2,3);
insert into s values (1,2,10);
explain (costs off) select * from r where b in (select b from s where c=10 order by r.c);
select * from r where b in (select b from s where c=10 order by r.c);
explain (costs off) select * from r where b in (select b from s where c=10 order by r.c limit 2);
select * from r where b in (select b from s where c=10 order by r.c limit 2);
explain (costs off) select * from r where b in (select b from s where c=10 order by r.c, b);
select * from r where b in (select b from s where c=10 order by r.c, b);
explain (costs off) select * from r where b in (select b from s where c=10 order by r.c, b limit 2);
select * from r where b in (select b from s where c=10 order by r.c, b limit 2);
explain (costs off) select * from r where b in (select b from s where c=10 order by c);
select * from r where b in (select b from s where c=10 order by c);
explain (costs off) select * from r where b in (select b from s where c=10 order by c limit 2);
select * from r where b in (select b from s where c=10 order by c limit 2);
