--
-- Drop existing table
--
DROP TABLE IF EXISTS foo;

--
-- Create new table foo
--
CREATE TABLE foo(type INTEGER, prod VARCHAR, quantity NUMERIC);

--
-- Insert some values
--
INSERT INTO foo VALUES
  (1, 'Table', 100),
  (2, 'Chair', 250),
  (3, 'Bed', 300);

--
-- Select query with grouping sets
--
SELECT type, prod, sum(quantity) s_quant
FROM
(
  SELECT type, prod, quantity
  FROM foo F1
  LIMIT 3
) F2 GROUP BY GROUPING SETS((type, prod), (prod)) ORDER BY type, s_quant;


--
-- Case for partitioned table
--

--
-- Create new table bar
--
CREATE TABLE pfoo(type INTEGER, prod VARCHAR, quantity NUMERIC)
DISTRIBUTED RANDOMLY
PARTITION BY RANGE (quantity) (
	partition "1" start (0) end (100),
	partition "2" start (100) end (200),
	partition "3" start (200) end (300),
	partition "4" start (300) end (400)
);

--
-- Insert some values
--
INSERT INTO pfoo VALUES
  (1, 'Table', 100),
  (2, 'Chair', 250),
  (3, 'Bed', 300);

--
-- Turn off GPORCA
--
set optimizer to off;

--
-- Select query with grouping sets
--
SELECT type, prod, sum(quantity) s_quant
FROM (
	SELECT * FROM pfoo
) AS t
GROUP BY GROUPING SETS((type), (prod))
ORDER BY type, s_quant;


---
--- Planning for sub-queries that have grouping sets
---

explain (costs off) WITH table1 AS (
	SELECT 2 AS city_id, 5 AS cnt
	UNION ALL
	SELECT 2 AS city_id, 1 AS cnt
	UNION ALL
	SELECT 3 AS city_id, 2 AS cnt
	UNION ALL
	SELECT 3 AS city_id, 7 AS cnt
),
fin AS (
SELECT
	coalesce(country_id, city_id) AS location_id,
	total
FROM (
	SELECT
	1 as country_id,
	city_id,
	sum(cnt) as total
	FROM table1
	GROUP BY GROUPING SETS (1,2)
	) base
)
SELECT *
FROM fin
WHERE location_id = 1;


--
-- Select constant from GROUPING SETS of multiple empty sets
--
explain (costs off)
select 1 from foo group by grouping sets ((), ());
select 1 from foo group by grouping sets ((), ());

--
-- GROUPING SETS with const in the group by
--
create table foo_gset_const(a int);
insert into foo_gset_const values(0), (1);
-- const and var in the groupint sets
explain (costs off)
select 1, a from foo_gset_const group by grouping sets(1,2);
select 1, a from foo_gset_const group by grouping sets(1,2);

explain (costs off)
select 1, a, count(distinct(a)) from foo_gset_const group by grouping sets(1,2);
select 1, a, count(distinct(a)) from foo_gset_const group by grouping sets(1,2);

explain (costs off)
select * from (select 1 as x, a, sum(a) as sum from foo_gset_const group by grouping sets(1, 2)) ss where x = 1 and sum = 1;
select * from (select 1 as x, a, sum(a) as sum from foo_gset_const group by grouping sets(1, 2)) ss where x = 1 and sum = 1;
-- only const in the groupint sets
explain (costs off)
select '' ,'' ,count(1) from foo_gset_const group by rollup(1,2) ;
select '' ,'' ,count(1) from foo_gset_const group by rollup(1,2) ;

explain (costs off)
select '' ,'' ,count(distinct(a)) from foo_gset_const group by rollup(1,2) ;
select '' ,'' ,count(distinct(a)) from foo_gset_const group by rollup(1,2) ;
drop table foo_gset_const;


--
-- GROUPING SETS with DQA should not have unnecessary sort nodes
--
create table foo_gset_dqa(i int, j int);
insert into foo_gset_dqa values(1,1);
insert into foo_gset_dqa values(2,1);

explain (costs off)
select i, j, count(distinct j) from foo_gset_dqa GROUP BY grouping sets((i), (j));
select i, j, count(distinct j) from foo_gset_dqa GROUP BY grouping sets((i), (j));

drop table foo_gset_dqa;

--
-- Reset settings
--
reset optimizer;

