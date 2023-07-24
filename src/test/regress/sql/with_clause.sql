-- start_matchsubs
--
-- m/ERROR:  Too much references to non-SELECT CTE \(allpaths\.c:\d+\)/
-- s/\d+/XXX/g
--
-- end_matchsubs

drop table if exists with_test1 cascade;
create table with_test1 (i int, t text, value int) distributed by (i);
insert into with_test1 select i%10, 'text' || i%20, i%30 from generate_series(0, 99) i;

drop table if exists with_test2 cascade;
create table with_test2 (i int, t text, value int);
insert into with_test2 select i%100, 'text' || i%200, i%300 from generate_series(0, 999) i;

-- With clause with one common table expression
--begin_equivalent
with my_sum(total) as (select sum(value) from with_test1)
select *
from my_sum;

select sum(value) as total from with_test1;
--end_equivalent

-- With clause with two common table expression
--begin_equivalent
with my_sum(total) as (select sum(value) from with_test1),
     my_count(cnt) as (select count(*) from with_test1)
select cnt, total
from my_sum, my_count;

select cnt, total
from (select sum(value) as total from with_test1) tmp1,
     (select count(*) as cnt from with_test1) tmp2;
--end_equivalent

-- With clause with one common table expression that is referenced twice
--begin_equivalent
with my_group_sum(i, total) as (select i, sum(value) from with_test1 group by i)
select gs1.i, gs1.total, gs2.total
from my_group_sum gs1, my_group_sum gs2
where gs1.i = gs2.i + 1;

select gs1.i, gs1.total, gs2.total
from (select i, sum(value) as total from with_test1 group by i) gs1,
     (select i, sum(value) as total from with_test1 group by i) gs2
where gs1.i = gs2.i + 1;
--end_equivalent

-- With clause with one common table expression that contains the other common table expression
--begin_equivalent
with my_count(i, cnt) as (select i, count(*) from with_test1 group by i),
     my_sum(total) as (select sum(cnt) from my_count)
select *
from my_sum;

select sum(cnt) as total from (select i, count(*) as cnt from with_test1 group by i) my_count;
--end_equivalent

-- WITH query contains WITH
--begin_equivalent
with my_sum(total) as (
     with my_group_sum(total) as (select sum(value) from with_test1 group by i)
     select sum(total) from my_group_sum)
select *
from my_sum;

select sum(total) from (select sum(value) as total from with_test1 group by i) my_group_sum;
--end_equivalent

-- pathkeys
with my_order as (select * from with_test1 order by i)
select i, count(*)
from my_order
group by i order by i;

-- WITH query used in InitPlan
--begin_equivalent
with my_max(maximum) as (select max(value) from with_test1)
select * from with_test2
where value < (select * from my_max);

select * from with_test2
where value < (with my_max(maximum) as (select max(value) from with_test1)
               select * from my_max);

select * from with_test2
where value < (select max(value) from with_test1);
--end_equivalent

-- WITH query used in InitPlan and the main query at the same time
--begin_equivalent
with my_max(maximum) as (select max(value) from with_test1)
select with_test2.* from with_test2, my_max
where value < (select * from my_max)
and i < maximum and i > maximum - 10;

select with_test2.* from with_test2, (select max(value) as maximum from with_test1) as my_max
where value < (select max(value) from with_test1)
and i < maximum and i > maximum - 10;
--end_equivalent

-- WITH query used in subplan
--begin_equivalent
with my_groupmax(i, maximum) as (select i, max(value) from with_test1 group by i)
select * from with_test2
where value < all (select maximum from my_groupmax);

select * from with_test2
where value < all (select max(value) from with_test1 group by i);
--end_equivalent

-- WITH query used in subplan and the main query at the same time
--begin_equivalent
with my_groupmax(i, maximum) as (select i, max(value) from with_test1 group by i)
select * from with_test2, my_groupmax
where with_test2.i = my_groupmax.i
and value < all (select maximum from my_groupmax);

select * from with_test2, (select i, max(value) as maximum from with_test1 group by i) as my_groupmax
where with_test2.i = my_groupmax.i
and value < all (select max(value) from with_test1 group by i);
--end_equivalent

--begin_equivalent
with my_groupmax(i, maximum) as (select i, max(value) from with_test1 group by i)
SELECT count(*) FROM my_groupmax WHERE maximum > (SELECT sum(maximum)/100 FROM my_groupmax);

select count(*) from (select i, max(value) as maximum from with_test1 group by i) as my_groupmax
where maximum > (SELECT sum(maximum)/100 FROM (select i, max(value) as maximum from with_test1 group by i) as tmp);
--end_equivalent

-- name resolution
--begin_equivalent
with my_max(maximum) as (select max(value) from with_test2)
select * from with_test1, my_max
where value < (with my_max(maximum) as (select max(i) from with_test1)
               select * from my_max);

select * from with_test1, (select max(value) as maximum from with_test2) as my_max
where value < (select max(i) from with_test1);
--end_equivalent

-- INSERT
insert into with_test2
with my_sum(i, total) as (select i, sum(value) from with_test1 group by i)
select i, i || '', total
from my_sum;

-- CREATE TABLE AS
drop table if exists with_test3;
create table with_test3 as
with my_sum(i, total) as (select i, sum(value) from with_test1 group by i)
select *
from my_sum;

-- view
drop view if exists my_view;
create view my_view (total) as
with my_sum(total) as (select sum(value) from with_test1)
select *
from my_sum;

SELECT pg_get_viewdef('my_view'::regclass);
SELECT pg_get_viewdef('my_view'::regclass, true);

drop view if exists my_view;
create view my_view(total) as
with my_sum(total) as (
     with my_group_sum(total) as (select sum(value) from with_test1 group by i)
     select sum(total) from my_group_sum)
select *
from my_sum;

SELECT pg_get_viewdef('my_view'::regclass);
SELECT pg_get_viewdef('my_view'::regclass, true);

drop view if exists my_view;
create view my_view(i, total) as (
    select i, sum(value) from with_test1 group by i);
with my_sum(total) as (select sum(total) from my_view)
select * from my_sum;

-- WITH query not used in the main query
--begin_equivalent
with my_sum(total) as (select sum(value) from with_test1)
select count(*) from with_test2;

select count(*) from with_test2;
--end_equivalent

-- WITH used in CURSOR query
begin;
	declare c cursor for with my_sum(total) as (select sum(value) from with_test1 group by i) select * from my_sum order by 1;
	fetch 10 from c;
	close c;
end;

-- Returning
create temporary table y (i int);
insert into y
with t as (select i from with_test1)
select i+20 from t returning *;

select * from y;

drop table y;

-- WITH used in SETOP
with my_sum(total) as (select sum(value) from with_test1)
select * from my_sum
union all
select * from my_sum;

-- ERROR cases

-- duplicate CTE name
with my_sum(total) as (select sum(value) from with_test1),
     my_sum(group_total) as (select sum(value) from with_test1 group by i)
select *
from my_sum;

-- INTO clause
with my_sum(total) as (select sum(value) from with_test1 into total_value)
select *
from my_sum;

-- name resolution
select * from with_test1, my_max
where value < (with my_max(maximum) as (select max(i) from with_test1)
               select * from my_max);

with my_sum(total) as (select sum(total) from my_group_sum),
     my_group_sum(i, total) as (select i, sum(total) from with_test1 group by i)
select *
from my_sum;

-- two WITH clauses
with my_sum(total) as (select sum(total) from with_test1),
with my_group_sum(i, total) as (select i, sum(total) from with_test1 group by i)
select *
from my_sum;

-- Test behavior with an unknown-type literal in the WITH
WITH q AS (SELECT 'foo' AS x)
SELECT x, x IS OF (unknown) as is_unknown FROM q;

with cte(foo) as ( select 42 ) select * from ((select foo from cte)) q;

select ( with cte(foo) as ( values(i) )
         select (select foo from cte) )
from with_test1
order by 1 limit 10;

select ( with cte(foo) as ( values(i) )
         values((select foo from cte)) )
from with_test1
order by 1 limit 10;

-- WITH query using Window functions
--begin_equivalent
with my_rank as (select i, t, value, rank() over (order by value) from with_test1)
select my_rank.* from with_test2, my_rank
where with_test2.i = my_rank.i
order by my_rank.i, my_rank.t, my_rank.value limit 100; -- order 1,2,3

select my_rank.* from with_test2, (select i, t, value, rank() over (order by value) from with_test1) as my_rank
where with_test2.i = my_rank.i
order by my_rank.i, my_rank.t, my_rank.value limit 100; -- order 1,2,3
--end_equivalent

-- WITH query and CSQ
--begin_equivalent
with my_group_sum(i, total) as (select i, sum(value) from with_test1 group by i)
select with_test2.* from with_test2
where value < any (select total from my_group_sum where my_group_sum.i = with_test2.i);

select with_test2.* from with_test2
where value < any (select total from (select i, sum(value) as total from with_test1 group by i) as tmp where tmp.i = with_test2.i);
--end_equivalent

--begin_equivalent
with my_group_sum(i, total) as (select i, sum(value) from with_test1 group by i)
select with_test2.* from with_test2, my_group_sum
where value < any (select total from my_group_sum where my_group_sum.i = with_test2.i)
and with_test2.i = my_group_sum.i;

select with_test2.* from with_test2, (select i, sum(value) from with_test1 group by i) as my_group_sum
where value < any (select total from (select i, sum(value) as total from with_test1 group by i) as tmp where tmp.i = with_test2.i)
and with_test2.i = my_group_sum.i;
--end_equivalent

--begin_equivalent
with my_group_sum(i, total) as (select i, sum(value) from with_test1 group by i)
select with_test2.* from with_test2
where value < all (select total from my_group_sum where my_group_sum.i = with_test2.i)
order by 1,2,3
limit 60; --order 1,2,3

select with_test2.* from with_test2
where value < all (select total from (select i, sum(value) as total from with_test1 group by i) as tmp where tmp.i = with_test2.i)
order by 1,2,3
limit 60; --order 1,2,3
--end_equivalent

drop table if exists d;
drop table if exists b;
create table with_b (i integer) distributed by (i);
insert into with_b values (1), (2);

--begin_equivalent
with b1 as (select * from with_b) select * from (select * from b1 where b1.i =1) AS FOO, b1 FOO2;

select * from (select * from (select * from with_b) as b1 where b1.i = 1) AS FOO, (select * from with_b) as foo2;
--end_equivalent
-- qual push down test
explain (costs off) with t as (select * from with_test1) select * from t where i = 10;

-- Test to validate an old bug which caused incorrect results when a subquery
-- in the WITH clause appears under a nested-loop join in the query plan when
-- gp_cte_sharing was set to off. (MPP-17848)
CREATE TABLE x (a integer) DISTRIBUTED BY (a);
insert into x values(1), (2);

CREATE TABLE y (m integer NOT NULL, n smallint) DISTRIBUTED BY (m);
insert into y values(10, 1);
insert into y values(20, 1);

with yy as (
   select m
   from y,
        (select 1 as p) iv
   where n = iv.p
)
select * from x, yy;

-- Check that WITH query is run to completion even if outer query isn't.
-- This is a test which exists in the upstream 'with' test suite in a section
-- which is currently under an ignore block. It has been copied here to avoid
-- merge conflicts since enabling it in the upstream test suite would require
-- altering the test output (as it depends on earlier tests which are failing
-- in GPDB currently).
DELETE FROM y;
INSERT INTO y SELECT generate_series(1,15) m;
WITH t AS (
    UPDATE y SET m = m * 100 RETURNING *
)
SELECT m BETWEEN 100 AND 1500 FROM t LIMIT 1;

SELECT * FROM y;

-- Nested RECURSIVE queries with double self-referential joins are planned by
-- joining two WorkTableScans, which GPDB cannot do yet. Ensure that we error
-- out with a descriptive message.
WITH RECURSIVE r1 AS (
	SELECT 1 AS a
	UNION ALL
	(
		WITH RECURSIVE r2 AS (
			SELECT 2 AS b
			UNION ALL
			SELECT b FROM r1, r2
		)
		SELECT b FROM r2
	)
)
SELECT * FROM r1 LIMIT 1;

-- GPDB
-- Greenplum does not support window functions in recursive part's target list
-- See issue https://github.com/greenplum-db/gpdb/issues/13299 for details.
-- Previously the following SQL will PANIC or Assert Fail if compiled with assert.
create table t_window_ordered_set_agg_rte(a bigint, b bigint, c bigint);

insert into t_window_ordered_set_agg_rte select i,i,i from generate_series(1, 10)i;

-- should error out during parse-analyze
with recursive rcte(x,y) as
(
  select a, b from t_window_ordered_set_agg_rte
  union all
  select (first_value(c) over (partition by b))::int, a+x
  from rcte,
       t_window_ordered_set_agg_rte as t
  where t.b = x
)
select * from rcte limit 10;

-- should error out during parse-analyze
with recursive rcte(x,y) as
(
  select a, b from t_window_ordered_set_agg_rte
  union all
  select first_value(c) over (partition by b), a+x
  from rcte,
       t_window_ordered_set_agg_rte as t
  where t.b = x
)
select * from rcte limit 10;

-- ensure orca doesn't fail (on build with asserts) when one cte on the coordinator has the correct flow and another doesn't
CREATE TABLE d (a int, b int, c int) DISTRIBUTED BY (a);
CREATE TABLE r (a int, b int, c char(255)) DISTRIBUTED REPLICATED;
INSERT INTO d SELECT 1, generate_series(1,10), 1;
INSERT INTO r SELECT 1, 2, generate_series(1,100);
SET gp_cte_sharing TO on;
EXPLAIN (ANALYZE off, COSTS off, VERBOSE off)
WITH e AS (
    SELECT DISTINCT b FROM d
), h AS (
    SELECT a FROM d JOIN e f USING (b) JOIN e USING (b)
) SELECT * FROM r JOIN h USING (a) JOIN h i USING (a);
DROP TABLE d;
DROP TABLE r;

-- Test planner not pushing down quals to non-SELECT queries inside CTE. There
-- can be a DML operation, and it's incorrect to push down upper quals to it.
--start_ignore
drop table if exists with_dml;
--end_ignore
create table with_dml (i int, j int) distributed by (i);
explain (costs off)
with cte as (
    insert into with_dml select i, i * 100 from generate_series(1,5) i
    returning i
) select count(*) from cte where i > 2;
with cte as (
    insert into with_dml select i, i * 100 from generate_series(1,5) i
    returning i
) select count(*) from cte where i > 2;
select count(*) c from with_dml;

-- Test one cannot use DML CTE if multiple CTE references found.
-- Otherwise it will cause duplicated DML operations or planner errors.
explain (costs off)
with cte as (
    insert into with_dml select i, i * 100 from generate_series(1,5) i
    returning i
) select count(*) from cte where i < (select avg(i) from cte);
explain (costs off)
with cte as (
    update with_dml set j = j + 1
    returning i
) select count(*) from cte where i < (select avg(i) from cte);
explain (costs off)
with cte as (
    delete from with_dml where i > 0
    returning i
) select count(*) from cte where i < (select avg(i) from cte);
drop table with_dml;
