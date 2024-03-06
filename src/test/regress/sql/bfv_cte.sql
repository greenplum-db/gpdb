--
-- Test queries mixes window functions with aggregate functions or grouping.
--
set optimizer_trace_fallback=on;
DROP TABLE IF EXISTS test_group_window;

CREATE TABLE test_group_window(c1 int, c2 int);

WITH tt AS (SELECT * FROM test_group_window)
SELECT tt.c1, COUNT() over () as fraction
FROM tt
GROUP BY tt.c1
ORDER BY tt.c1;

DROP TABLE test_group_window;

--
-- Set up
--
CREATE TABLE bfv_cte_foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
CREATE TABLE bfv_cte_bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

--
-- Test with CTE inlining disabled
--
set optimizer_cte_inlining = off;

--
-- With clause select test 1
--
WITH t AS
(
 SELECT e.*,f.*
 FROM
    (
      SELECT * FROM bfv_cte_foo WHERE a < 10
    ) e
 LEFT OUTER JOIN
    (
       SELECT * FROM bfv_cte_bar WHERE c < 10
    ) f

  ON e.a = f.d )
SELECT t.a,t.d, count(*) over () AS window
FROM t
GROUP BY t.a,t.d ORDER BY t.a,t.d LIMIT 2;

--
-- With clause select test 2
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT t.b,avg(t.a), rank() OVER (PARTITION BY t.a ORDER BY t.a) FROM bfv_cte_foo,t GROUP BY bfv_cte_foo.a,bfv_cte_foo.b,t.b,t.a ORDER BY 1,2,3 LIMIT 5;

--
-- With clause select test 3
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT cup.*, SUM(t.d) OVER(PARTITION BY t.b) FROM
  (
    SELECT bfv_cte_bar.*, AVG(t.b) OVER(PARTITION BY t.a ORDER BY t.b desc) AS e FROM t,bfv_cte_bar
  ) AS cup,
t WHERE cup.e < 10
GROUP BY cup.c,cup.d, cup.e ,t.d, t.b
ORDER BY 1,2,3,4
LIMIT 10;

--
-- With clause select test 4
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT cup.*, SUM(t.d) FROM
  (
    SELECT bfv_cte_bar.*, count(*) OVER() AS e FROM t,bfv_cte_bar WHERE t.a = bfv_cte_bar.c
  ) AS cup,
t GROUP BY cup.c,cup.d, cup.e,t.a
HAVING AVG(t.d) < 10 ORDER BY 1,2,3,4 LIMIT 10;

--
-- With clause select test 5
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT cup.*, SUM(t.d) OVER(PARTITION BY t.b) FROM
  (
    SELECT bfv_cte_bar.c as e,r.d FROM
		(
			SELECT t.d, avg(t.a) over() FROM t
		) r,bfv_cte_bar
  ) AS cup,
t WHERE cup.e < 10
GROUP BY cup.d, cup.e, t.d, t.b
ORDER BY 1,2,3
LIMIT 10;

--
-- Test with CTE inlining enabled
--
set optimizer_cte_inlining = on;
set optimizer_cte_inlining_bound = 1000;

--
-- With clause select test 1
--
WITH t AS
(
 SELECT e.*,f.*
 FROM
    (
      SELECT * FROM bfv_cte_foo WHERE a < 10
    ) e
 LEFT OUTER JOIN
    (
       SELECT * FROM bfv_cte_bar WHERE c < 10
    ) f

  ON e.a = f.d )
SELECT t.a,t.d, count(*) over () AS window
FROM t
GROUP BY t.a,t.d ORDER BY t.a,t.d LIMIT 2;

--
-- With clause select test 2
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT t.b,avg(t.a), rank() OVER (PARTITION BY t.a ORDER BY t.a) FROM bfv_cte_foo,t GROUP BY bfv_cte_foo.a,bfv_cte_foo.b,t.b,t.a ORDER BY 1,2,3 LIMIT 5;

--
-- With clause select test 3
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT cup.*, SUM(t.d) OVER(PARTITION BY t.b) FROM
  (
    SELECT bfv_cte_bar.*, AVG(t.b) OVER(PARTITION BY t.a ORDER BY t.b desc) AS e FROM t,bfv_cte_bar
  ) AS cup,
t WHERE cup.e < 10
GROUP BY cup.c,cup.d, cup.e ,t.d, t.b
ORDER BY 1,2,3,4
LIMIT 10;

--
-- With clause select test 4
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT cup.*, SUM(t.d) FROM
  (
    SELECT bfv_cte_bar.*, count(*) OVER() AS e FROM t,bfv_cte_bar WHERE t.a = bfv_cte_bar.c
  ) AS cup,
t GROUP BY cup.c,cup.d, cup.e,t.a
HAVING AVG(t.d) < 10 ORDER BY 1,2,3,4 LIMIT 10;

--
-- With clause select test 5
--
WITH t(a,b,d) AS
(
  SELECT bfv_cte_foo.a,bfv_cte_foo.b,bfv_cte_bar.d FROM bfv_cte_foo,bfv_cte_bar WHERE bfv_cte_foo.a = bfv_cte_bar.d
)
SELECT cup.*, SUM(t.d) OVER(PARTITION BY t.b) FROM
  (
    SELECT bfv_cte_bar.c as e,r.d FROM
		(
			SELECT t.d, avg(t.a) over() FROM t
		) r,bfv_cte_bar
  ) AS cup,
t WHERE cup.e < 10
GROUP BY cup.d, cup.e, t.d, t.b
ORDER BY 1,2,3
LIMIT 10;

DROP TABLE IF EXISTS bfv_cte_foo;
DROP TABLE IF EXISTS bfv_cte_bar;
reset optimizer_cte_inlining;
reset optimizer_cte_inlining_bound;

--
-- Test for an old bug with rescanning window functions. This needs to use
-- catalog tables, otherwise the plan will contain a Motion node that
-- materializes the result, and masks the problem.
--
with t (a,b,d) as (select 1,2,1 from pg_class limit 1)
SELECT cup.* FROM
 t, (
SELECT sum(t.b) OVER(PARTITION BY t.a ) AS e FROM (select 1 as a, 2 as b from pg_class limit 1)foo,t
) as cup
GROUP BY cup.e;

-- The following tests ensures queries involving CTE don't hang in ORCA or fall back.
-- Query hang is caused by a mismatch between the number of CTE producers and
-- consumers. Either there are more producers than consumers, or more consumers than
-- producers, the unconsumed producers or the starved consumers would cause the query
-- to hang. Query fall back, on the other hand, is due to a missing plan that satisfies
-- all the required plan properties. The issues stated above are fixed by sending
-- appropriate distribution requests based on how data is distributed to begin with,
-- to ensure the number of CTE producers and consumers match. For each of the tests,
-- we use explain to verify the plan and the actual query to verify the query doesn't
-- hang.

-- This test involves a tainted-replicated CTE and two consumer 
-- To ensure there's no duplicate hazard, the cost model chooses a plan that gathers
-- the CTE onto the coordinator. Both the producer and the consumer executes on the
-- coordiator. There's 1 producer matching 1 consumer.
drop table if exists rep;
create table rep (i character varying(10)) distributed replicated;

explain (analyze off, costs off, verbose off)
with cte1 as ( select *,row_number() over ( partition by i) as rank_desc from rep),
cte2 as ( select 'col1' tblnm,count(*) diffcnt from ( select * from cte1) x)
select * from ( select 'col1' tblnm from cte1) a left join cte2 c on a.tblnm=c.tblnm;

with cte1 as ( select *,row_number() over ( partition by i) as rank_desc from rep),
     cte2 as ( select 'col1' tblnm,count(*) diffcnt from ( select * from cte1) x)
select * from ( select 'col1' tblnm from cte1) a left join cte2 c on a.tblnm=c.tblnm;
drop table rep;

-- This test involves a tainted-replicated CTE and two consumers
-- To ensure there's no duplicate hazard, the cost model chooses a plan that executes
-- the producer and both consumers on one segment, before gathering the data onto
-- the coordinator and return. There's 1 producer matching 1 consumer for each consumer.
drop table if exists rep1, rep2;
create table rep1 (id bigserial not null, isc varchar(15) not null,iscd varchar(15) null) distributed replicated;
create table rep2 (id numeric null, rc varchar(255) null,ri numeric null) distributed replicated;
insert into rep1 (isc,iscd) values ('cmn_bin_yes', 'cmn_bin_yes');
insert into rep2 (id,rc,ri) values (113551,'cmn_bin_yes',101991), (113552,'cmn_bin_no',101991), (113553,'cmn_bin_err',101991), (113554,'cmn_bin_null',101991);
explain (analyze off, costs off, verbose off)
with
    t1 as (select * from rep1),
    t2 as (select id, rc from rep2 where ri = 101991)
select p.*from t1 p join t2 r on p.isc = r.rc join t2 r1 on p.iscd = r1.rc;

with
    t1 as (select * from rep1),
    t2 as (select id, rc from rep2 where ri = 101991)
select p.*from t1 p join t2 r on p.isc = r.rc join t2 r1 on p.iscd = r1.rc limit 1;
drop table rep1, rep2;

-- This test involves a strictly replicated CTE and two consumers
-- To ensure there's no duplicate hazard, the cost model chooses a plan that executes
-- the producer and both consumers on one segment by placing a one-time segment filter
-- on the producer side. The coordinator gathers data from all three segments, even 
-- though only one segment has the tuples from the join due to the one-time filter. 
-- There's 1 producer matching 1 consumer for each consumer.
drop table if exists dist1, dist2, rep;
create table dist1 (a int, b int);
create table dist2 (a int, b int);
create table rep (a int, b int) distributed replicated;
insert into dist1 select 1, generate_series(1,10);
insert into dist2 select 1, generate_series(1,20);
insert into rep select 1, 1;

explain (analyze off, costs off, verbose off)
with t1_cte as (select b from dist1),
rep_cte as (select a from rep)
select
case when (dist2.b in (1,2)) then (select rep_cte.a from rep_cte)
when (dist2.b in (1,2)) then (select rep_cte.a from rep_cte)
end as rep_cte_a
from t1_cte join dist2 on t1_cte.b = dist2.b;

with t1_cte as (select b from dist1),
rep_cte as (select a from rep)
select
case when (dist2.b in (1,2)) then (select rep_cte.a from rep_cte)
when (dist2.b in (1,2)) then (select rep_cte.a from rep_cte)
end as rep_cte_a
from t1_cte join dist2 on t1_cte.b = dist2.b;
drop table dist1, dist2, rep;

--
-- Test for a bug in ORCA optimizing a CTE view.
--
-- This crashed at one point in retail build due to preprocessor creating a
-- duplicate CTE anchor. That led ORCA to construct a bad plan where
-- CTEConsumer project list contained an invalid scalar subplan and caused a
-- SIGSEGV during DXL to PlStmt translation.
--
create table a_table(a smallint);

create view cte_view as
  with t1 as (select a from a_table)
  select t1.a from t1
  where (t1.a = (select t1.a from t1));

-- Only by tampering with pg_stats directly I was able to guide ORCA cost model
-- to pick a plan that would crash before this fix
set allow_system_table_mods=true;
update pg_class set relpages = 1::int, reltuples = 12.0::real where relname = 'a_table';
reset allow_system_table_mods;

explain select * from a_table join cte_view on a_table.a = (select a from cte_view) where cte_view.a = 2024;

reset optimizer_trace_fallback;
