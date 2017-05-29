--
-- UNION (also INTERSECT, EXCEPT)
--

-- Simple UNION constructs

SELECT 1 AS two UNION SELECT 2;

SELECT 1 AS one UNION SELECT 1;

SELECT 1 AS two UNION ALL SELECT 2;

SELECT 1 AS two UNION ALL SELECT 1;

SELECT 1 AS three UNION SELECT 2 UNION SELECT 3;

SELECT 1 AS two UNION SELECT 2 UNION SELECT 2;

SELECT 1 AS three UNION SELECT 2 UNION ALL SELECT 2;

SELECT 1.1 AS two UNION SELECT 2.2;

-- Mixed types

SELECT 1.1 AS two UNION SELECT 2;

SELECT 1 AS two UNION SELECT 2.2;

SELECT 1 AS one UNION SELECT 1.0::float8;

SELECT 1.1 AS two UNION ALL SELECT 2;

SELECT 1.0::float8 AS two UNION ALL SELECT 1;

SELECT 1.1 AS three UNION SELECT 2 UNION SELECT 3;

SELECT 1.1::float8 AS two UNION SELECT 2 UNION SELECT 2.0::float8;

SELECT 1.1 AS three UNION SELECT 2 UNION ALL SELECT 2;

SELECT 1.1 AS two UNION (SELECT 2 UNION ALL SELECT 2);

--
-- Try testing from tables...
--

SELECT f1 AS five FROM FLOAT8_TBL
UNION
SELECT f1 FROM FLOAT8_TBL;

SELECT f1 AS ten FROM FLOAT8_TBL
UNION ALL
SELECT f1 FROM FLOAT8_TBL;

SELECT f1 AS nine FROM FLOAT8_TBL
UNION
SELECT f1 FROM INT4_TBL;

SELECT f1 AS ten FROM FLOAT8_TBL
UNION ALL
SELECT f1 FROM INT4_TBL;

SELECT f1 AS five FROM FLOAT8_TBL
  WHERE f1 BETWEEN -1e6 AND 1e6
UNION
SELECT f1 FROM INT4_TBL
  WHERE f1 BETWEEN 0 AND 1000000;

SELECT CAST(f1 AS char(4)) AS three FROM VARCHAR_TBL
UNION
SELECT f1 FROM CHAR_TBL;

SELECT f1 AS three FROM VARCHAR_TBL
UNION
SELECT CAST(f1 AS varchar) FROM CHAR_TBL;

SELECT f1 AS eight FROM VARCHAR_TBL
UNION ALL
SELECT f1 FROM CHAR_TBL;

SELECT f1 AS five FROM TEXT_TBL
UNION
SELECT f1 FROM VARCHAR_TBL
UNION
SELECT TRIM(TRAILING FROM f1) FROM CHAR_TBL;

--
-- INTERSECT and EXCEPT
--

SELECT q2 FROM int8_tbl INTERSECT SELECT q1 FROM int8_tbl;

SELECT q2 FROM int8_tbl INTERSECT ALL SELECT q1 FROM int8_tbl;

SELECT q2 FROM int8_tbl EXCEPT SELECT q1 FROM int8_tbl;

SELECT q2 FROM int8_tbl EXCEPT ALL SELECT q1 FROM int8_tbl;

SELECT q2 FROM int8_tbl EXCEPT ALL SELECT DISTINCT q1 FROM int8_tbl;

SELECT q1 FROM int8_tbl EXCEPT SELECT q2 FROM int8_tbl;

SELECT q1 FROM int8_tbl EXCEPT ALL SELECT q2 FROM int8_tbl;

SELECT q1 FROM int8_tbl EXCEPT ALL SELECT DISTINCT q2 FROM int8_tbl;

--
-- Mixed types
--

SELECT f1 FROM float8_tbl INTERSECT SELECT f1 FROM int4_tbl;

SELECT f1 FROM float8_tbl EXCEPT SELECT f1 FROM int4_tbl;

--
-- Operator precedence and (((((extra))))) parentheses
--

SELECT q1 FROM int8_tbl INTERSECT SELECT q2 FROM int8_tbl UNION ALL SELECT q2 FROM int8_tbl;

SELECT q1 FROM int8_tbl INTERSECT (((SELECT q2 FROM int8_tbl UNION ALL SELECT q2 FROM int8_tbl)));

(((SELECT q1 FROM int8_tbl INTERSECT SELECT q2 FROM int8_tbl))) UNION ALL SELECT q2 FROM int8_tbl;

SELECT q1 FROM int8_tbl UNION ALL SELECT q2 FROM int8_tbl EXCEPT SELECT q1 FROM int8_tbl;

SELECT q1 FROM int8_tbl UNION ALL (((SELECT q2 FROM int8_tbl EXCEPT SELECT q1 FROM int8_tbl)));

(((SELECT q1 FROM int8_tbl UNION ALL SELECT q2 FROM int8_tbl))) EXCEPT SELECT q1 FROM int8_tbl;

--
-- Subqueries with ORDER BY & LIMIT clauses
--

-- In this syntax, ORDER BY/LIMIT apply to the result of the EXCEPT
SELECT q1,q2 FROM int8_tbl EXCEPT SELECT q2,q1 FROM int8_tbl
ORDER BY q2,q1;

-- This should fail, because q2 isn't a name of an EXCEPT output column
SELECT q1 FROM int8_tbl EXCEPT SELECT q2 FROM int8_tbl ORDER BY q2 LIMIT 1;

-- But this should work:
SELECT q1 FROM int8_tbl EXCEPT (((SELECT q2 FROM int8_tbl ORDER BY q2 LIMIT 1))) ORDER BY 1;

--
-- New syntaxes (7.1) permit new tests
--

(((((select * from int8_tbl)))));

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

create or replace function count_operator(explain_query text, operator text) returns int as
$$
rv = plpy.execute(explain_query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

set optimizer = on;

--
-- N-ary UNION ALL results
-- @optimizer_mode on

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
-- @optimizer_mode on

select count_operator('
explain
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
explain
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
explain
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
explain
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
-- @optimizer_mode on

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
-- @optimizer_mode on

select count_operator('
explain
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
explain
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
explain
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
explain
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
-- @optimizer_mode on

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
-- @optimizer_mode on

select count_operator('explain (select a1 from T_a1) UNION ALL (select b1 from T_b2) order by 1;', 'APPEND');

select count_operator('explain (select b1 from T_b2) UNION ALL (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('explain (select a1 from T_a1) UNION ALL (select c1 from T_random) order by 1;', 'APPEND');

select count_operator('explain (select c1 from T_random) UNION ALL (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('explain (select * from T_a1) UNION ALL (select * from T_b2) order by 1;', 'APPEND');

select count_operator('explain (select * from T_a1) UNION ALL (select * from T_random) order by 1;', 'APPEND');

select count_operator('explain (select * from T_b2) UNION ALL (select * from T_random) order by 1;', 'APPEND');

select count_operator('explain
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select a1 from T_a1) UNION ALL (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('explain
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant) UNION ALL (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('explain
with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select c1 from T_random) UNION ALL (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('explain with T_constant (d1, d2) as(
SELECT 100, 100
UNION ALL SELECT 200, 200
UNION ALL SELECT 300, 300)
(select d1 from T_constant) UNION ALL (select c1 from T_random) order by 1;', 'APPEND');

--
-- Binary UNION results
-- @optimizer_mode on

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
-- @optimizer_mode on

select count_operator('explain (select a1 from T_a1) UNION (select b1 from T_b2) order by 1;', 'APPEND');

select count_operator('explain (select b1 from T_b2) UNION (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('explain (select a1 from T_a1) UNION (select c1 from T_random) order by 1;', 'APPEND');

select count_operator('explain (select c1 from T_random) UNION (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('explain (select * from T_a1) UNION (select * from T_b2) order by 1;', 'APPEND');

select count_operator('explain (select * from T_a1) UNION (select * from T_random) order by 1;', 'APPEND');

select count_operator('explain (select * from T_b2) UNION (select * from T_random) order by 1;', 'APPEND');

select count_operator('explain
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select a1 from T_a1) UNION (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('explain
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant) UNION (select a1 from T_a1) order by 1;', 'APPEND');

select count_operator('explain
with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select c1 from T_random) UNION (select d1 from T_constant) order by 1;', 'APPEND');

select count_operator('explain with T_constant (d1, d2) as(
SELECT 100, 100
UNION SELECT 200, 200
UNION SELECT 300, 300)
(select d1 from T_constant) UNION (select c1 from T_random) order by 1;', 'APPEND');


set optimizer = off;

--
-- N-ary UNION ALL results
-- @optimizer_mode off

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
-- N-ary UNION results
-- @optimizer_mode off

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
-- Binary UNION ALL results
-- @optimizer_mode off

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
-- Binary UNION results
-- @optimizer_mode off

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
-- Clean up
--

DROP TABLE IF EXISTS T_a1 CASCADE;
DROP TABLE IF EXISTS T_b2 CASCADE;
DROP TABLE IF EXISTS T_random CASCADE;

reset optimizer;
-- EOF
