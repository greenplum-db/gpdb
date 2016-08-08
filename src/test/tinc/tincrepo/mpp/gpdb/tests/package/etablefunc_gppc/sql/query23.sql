-- ETF sub-query contains correlated sub-query

select avg(a)::int,'haha'::text from t1;

SELECT * FROM transform( 
    TABLE( select avg(a)::int,'haha'::text from t1 ));

SELECT * FROM transform(
Table(select a,e from t1 t1 where d >
(select avg(a) from t1 t2 where t2.a=t1.a)
)) order by b;

-- This query should fail with following error message:
select a,e from t1 t1 where a >
(SELECT b FROM transform(
TABLE( select avg(a)::int,'haha'::text from t1 t2 where t2.a=t1.d)
)) ;
-- ERROR:  subquery in TABLE value expression may not refer to relation of another query level
-- LINE 3: TABLE( select avg(a)::int,'haha'::text from t1 t2 where t2.a...

-- The following internal correlation sub-query works correctly:
SELECT * FROM transform(
Table(select a,e from t1 t1 where a >
(select avg(a) from t1 t2 where t2.a=t1.d)
)) order by b;

select avg(a) from t1 
group by (select b from transform(TABLE(select a,e from t1 where a=10 )) );

SELECT min(b) FROM
transform( TABLE(select a,e from t1 where a <5) );

------------------------------
-- This query works correctly:
SELECT * FROM t1 WHERE a IN (
SELECT b FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 order by d;

-- The following query should also work.
-- The only different between this one the one above is this uses "NOT IN"
SELECT * FROM t1 WHERE a NOT IN (
SELECT b FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 order by a;

-------------------------------
-- For EXISTS and NOT EXISTS
SELECT * FROM t1 WHERE EXISTS (
SELECT 1 FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 ORDER BY a;

SELECT * FROM t1 WHERE NOT EXISTS (
SELECT 1 FROM transform(
TABLE(select a,e from t1
order by a
scatter randomly))
) AND a < 10 ORDER BY a;

--------------------------------
-- ETF in ALL sub query expression
SELECT * FROM t1 WHERE a>ALL (
SELECT b FROM transform(
TABLE(select a,e from t1
where a>90 and a<98
order by a
scatter randomly))
) ORDER BY a;

--------------------------------
-- Calling ETF from WITH clause (CTE)
WITH first_5 AS (
SELECT b,a FROM transform( TABLE(
select a,e from t1 where a <= 5
))
)
select * from first_5 order by b;

---------------------------------
-- Test ETF as argument
-- The following query should fail with error as shown:
SELECT * FROM ARRAY_APPEND(array['value0','value1'],
(select a from transform( TABLE(select * from intable))));
-- ERROR:  more than one row returned by a subquery used as an expression

-- The following query should succeed
SELECT * FROM ARRAY_APPEND(array['value0','value1'],
(select a from transform( TABLE(select * from intable where id=2))));
