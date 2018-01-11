-- Additional GPDB-added tests for UNION

create temp table t_union1 (a int, b int);
select distinct a, null::integer as c from t_union1 union select a, b from t_union1;
drop table t_union1;

select null union select distinct null;

select 1 union select distinct null::integer;

select 1 a, NULL b, NULL c UNION SELECT 2, 3, NULL UNION SELECT 3, NULL, 4;

select ARRAY[1, 2, 3] union select distinct null::integer[];

select 1 intersect (select 1, 2 union all select 3, 4);
select 1 a, row_number() over (partition by 'a') union all (select 1 a , 2 b);

-- This should preserve domain types
select pg_typeof(a) from (select 'a'::information_schema.sql_identifier a union all
select 'b'::information_schema.sql_identifier)a;

(select * from (
     (select '1' as a union select null)
     union
     (select 1 union select distinct null::integer)
   )s)
  union
  (select * from (
     (select '1' union select null)
     union
     (select 1 union select distinct null::integer)
  )s2);

-- Yet, we keep behaviors on text-like columns
select pg_typeof(a) from(select 'foo' a union select 'foo'::name)s;
select pg_typeof(a) from(select 1 x, 'foo' a union
    select 1, 'foo' union select 1, 'foo'::name)s;
select pg_typeof(a) from(select 1 x, 'foo' a union
    (select 1, 'foo' union select 1, 'foo'::name))s;

CREATE TABLE union_ctas (a, b) AS SELECT 1, 2 UNION SELECT 1, 1 UNION SELECT 1, 1;
SELECT * FROM union_ctas;
DROP TABLE union_ctas;

-- MPP-21075: push quals below union
CREATE TABLE union_quals1 (a, b) AS SELECT i, i%2 from generate_series(1,10) i;
CREATE TABLE union_quals2 (a, b) AS SELECT i%2, i from generate_series(1,10) i;
SELECT * FROM (SELECT a, b from union_quals1 UNION SELECT b, a from union_quals2) as foo(a,b) where a > b order by a;
SELECT * FROM (SELECT a, max(b) over() from union_quals1 UNION SELECT * from union_quals2) as foo(a,b) where b > 6 order by a,b;

-- MPP-22266: different combinations of set operations and distinct
select * from ((select 1, 'A' from (select distinct 'B') as foo) union (select 1, 'C')) as bar;
select 1 union (select distinct null::integer union select '10');
select 1 union (select 2 from (select distinct null::integer union select 1) as x);
select 1 union (select distinct 10 from (select 1, 3.0 union select distinct 2, null::integer) as foo);
select 1 union (select distinct '10' from (select 1, 3.0 union select distinct 2, null::integer) as foo);
select distinct a from (select 'A' union select 'B') as foo(a);
select distinct a from (select distinct 'A' union select 'B') as foo(a);
select distinct a from (select distinct 'A' union select distinct 'B') as foo(a);
select distinct a from (select  'A' from (select distinct 'C' ) as bar union select distinct 'B') as foo(a);
select distinct a from (select  distinct 'A' from (select distinct 'C' ) as bar union select distinct 'B') as foo(a);
select distinct a from (select  distinct 'A' from (select 'C' from (select distinct 'D') as bar1 ) as bar union select distinct 'B') as foo(a);

--
-- Setup
--

--start_ignore
DROP TABLE IF EXISTS T_a1 CASCADE;
DROP TABLE IF EXISTS T_b2 CASCADE;
DROP TABLE IF EXISTS T_random CASCADE;
--end_ignore

CREATE TABLE T_a1 (a1 int, a2 int) DISTRIBUTED BY(a1);
INSERT INTO T_a1 SELECT i, i%5 from generate_series(1,10) i;

CREATE TABLE T_b2 (b1 int, b2 int) DISTRIBUTED BY(b2);
INSERT INTO T_b2 SELECT i, i%5 from generate_series(1,20) i;

CREATE TABLE T_random (c1 int, c2 int);
INSERT INTO T_random SELECT i, i%5 from generate_series(1,30) i;

create language plpythonu;

create or replace function count_operator(query text, operator text) returns int as
$$
rv = plpy.execute('EXPLAIN ' + query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

--
-- N-ary UNION ALL results
--

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select a1 from T_a1)
UNION ALL
(select b1 from T_b2)
UNION ALL
(select c1 from T_random)
UNION ALL
(select d1 from T_constant)
order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select b1 from T_b2)
UNION ALL
(select a1 from T_a1)
UNION ALL
(select c1 from T_random)
UNION ALL
(select d1 from T_constant)
order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select c1 from T_random)
UNION ALL
(select a1 from T_a1)
UNION ALL
(select b1 from T_b2)
UNION ALL
(select d1 from T_constant)
order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant)
UNION ALL
(select c1 from T_random)
UNION ALL
(select a1 from T_a1)
UNION ALL
(select b1 from T_b2)
order by 1;

--
-- N-ary UNION ALL explain
--

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select a1 from T_a1)
UNION ALL
(select b1 from T_b2)
UNION ALL
(select c1 from T_random)
UNION ALL
(select d1 from T_constant)
order by 1;'
, 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select b1 from T_b2)
UNION ALL
(select a1 from T_a1)
UNION ALL
(select c1 from T_random)
UNION ALL
(select d1 from T_constant)
order by 1;'
, 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select c1 from T_random)
UNION ALL
(select a1 from T_a1)
UNION ALL
(select b1 from T_b2)
UNION ALL
(select d1 from T_constant)
order by 1;'
, 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant)
UNION ALL
(select c1 from T_random)
UNION ALL
(select a1 from T_a1)
UNION ALL
(select b1 from T_b2)
order by 1;'
, 'APPEND');

--
-- N-ary UNION results
--

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select a1 from T_a1)
UNION
(select b1 from T_b2)
UNION
(select c1 from T_random)
UNION
(select d1 from T_constant)
order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select b1 from T_b2)
UNION
(select a1 from T_a1)
UNION
(select c1 from T_random)
UNION ALL
(select d1 from T_constant)
order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select c1 from T_random)
UNION
(select a1 from T_a1)
UNION
(select b1 from T_b2)
UNION ALL
(select d1 from T_constant)
order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant)
UNION ALL
(select c1 from T_random)
UNION
(select a1 from T_a1)
UNION
(select b1 from T_b2)
order by 1;

--
-- N-ary UNION explain
--

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select a1 from T_a1)
UNION
(select b1 from T_b2)
UNION
(select c1 from T_random)
UNION
(select d1 from T_constant)
order by 1;'
, 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select b1 from T_b2)
UNION
(select a1 from T_a1)
UNION
(select c1 from T_random)
UNION
(select d1 from T_constant)
order by 1;'
, 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select c1 from T_random)
UNION
(select a1 from T_a1)
UNION
(select b1 from T_b2)
UNION
(select d1 from T_constant)
order by 1;'
, 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant)
UNION
(select c1 from T_random)
UNION
(select a1 from T_a1)
UNION
(select b1 from T_b2)
order by 1;'
, 'APPEND');

--
-- Binary UNION ALL results
--

(select a1 from T_a1) UNION ALL (select b1 from T_b2) order by 1;

(select b1 from T_b2) UNION ALL (select a1 from T_a1) order by 1;

(select a1 from T_a1) UNION ALL (select c1 from T_random) order by 1;

(select c1 from T_random) UNION ALL (select a1 from T_a1) order by 1;

(select * from T_a1) UNION ALL (select * from T_b2) order by 1;

(select * from T_a1) UNION ALL (select * from T_random) order by 1;

(select * from T_b2) UNION ALL (select * from T_random) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select a1 from T_a1) UNION ALL (select d1 from T_constant) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant) UNION ALL (select a1 from T_a1) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select c1 from T_random) UNION ALL (select d1 from T_constant) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant) UNION ALL (select c1 from T_random) order by 1;

--
-- Binary UNION ALL explain
--

select count_operator('(select a1 from T_a1) UNION ALL (select b1 from T_b2) order by 1;', 'APPEND');

select count_operator('(select b1 from T_b2) UNION ALL (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('(select a1 from T_a1) UNION ALL (select c1 from T_random) order by 1;', 'APPEND');

select count_operator('(select c1 from T_random) UNION ALL (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('(select * from T_a1) UNION ALL (select * from T_b2) order by 1;', 'APPEND');

select count_operator('(select * from T_a1) UNION ALL (select * from T_random) order by 1;', 'APPEND');

select count_operator('(select * from T_b2) UNION ALL (select * from T_random) order by 1;', 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select a1 from T_a1) UNION ALL (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant) UNION ALL (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select c1 from T_random) UNION ALL (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant) UNION ALL (select c1 from T_random) order by 1;', 'APPEND');

--
-- Binary UNION results
--

(select a1 from T_a1) UNION (select b1 from T_b2) order by 1;

(select b1 from T_b2) UNION (select a1 from T_a1) order by 1;

(select a1 from T_a1) UNION (select c1 from T_random) order by 1;

(select c1 from T_random) UNION (select a1 from T_a1) order by 1;

(select * from T_a1) UNION (select * from T_b2) order by 1;

(select * from T_a1) UNION (select * from T_random) order by 1;

(select * from T_b2) UNION (select * from T_random) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select a1 from T_a1) UNION (select d1 from T_constant) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant) UNION (select a1 from T_a1) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select c1 from T_random) UNION (select d1 from T_constant) order by 1;

with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant) UNION (select c1 from T_random) order by 1;

--
-- Binary UNION explain
--

select count_operator('(select a1 from T_a1) UNION (select b1 from T_b2) order by 1;', 'APPEND');

select count_operator('(select b1 from T_b2) UNION (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('(select a1 from T_a1) UNION (select c1 from T_random) order by 1;', 'APPEND');

select count_operator('(select c1 from T_random) UNION (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('(select * from T_a1) UNION (select * from T_b2) order by 1;', 'APPEND');

select count_operator('(select * from T_a1) UNION (select * from T_random) order by 1;', 'APPEND');

select count_operator('(select * from T_b2) UNION (select * from T_random) order by 1;', 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select a1 from T_a1) UNION (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant) UNION (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select c1 from T_random) UNION (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant) UNION (select c1 from T_random) order by 1;', 'APPEND');

--
-- Clean up
--

DROP TABLE IF EXISTS T_a1 CASCADE;
DROP TABLE IF EXISTS T_b2 CASCADE;
DROP TABLE IF EXISTS T_random CASCADE;
