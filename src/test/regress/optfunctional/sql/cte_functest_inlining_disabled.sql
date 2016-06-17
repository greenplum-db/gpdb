
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema cte_functest;
set optimizer_cte_inlining = off;
set search_path to cte_functest;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_1.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test1: Single producer and single consumer

WITH v AS (SELECT a, b FROM foo WHERE b < 5)
SELECT * FROM v WHERE a = 1 ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_10.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test8b: CTE defined in the HAVING clause

WITH w AS (SELECT a, b FROM foo where b < 5)
SELECT a, sum(b) FROM foo
WHERE b > 1
GROUP BY a
HAVING sum(b) < ( WITH z AS (SELECT c FROM bar, w WHERE c = w.a AND c < 2) SELECT c+2 FROM z) ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_11.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test8b: CTE defined in the HAVING clause

WITH w AS (SELECT a, b FROM foo where b < 5)
SELECT a, sum(b) FROM foo
WHERE b > 1
GROUP BY a
HAVING sum(b) < ( WITH z AS (SELECT c FROM bar, w WHERE c = w.a AND c < 2) SELECT c+2 FROM z) ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_12.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test9: CTE defined inside another CTE

WITH v AS (WITH w AS (SELECT a, b FROM foo WHERE b < 5) 
SELECT w1.a, w2.b from w w1, w w2 WHERE w1.a = w2.a AND w1.a > 2)
SELECT v1.a, v2.a, v2.b
FROM v as v1, v as v2
WHERE v1.a = v2.a ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_13.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test10: Multi-level nesting

WITH v as (WITH x as (
                       SELECT * FROM foo WHERE b < 5
                     ) 
           SELECT x1.a ,x1.b FROM x x1, x x2 
           WHERE x1.a = x2.a AND x1.a = (WITH y as (
						     SELECT * FROM x
                                                   ) 
					SELECT max(y1.b) FROM y y1, y y2 WHERE y1.a < y2.a)) 
SELECT * FROM v v1, v v2 WHERE v1.a < v2.b ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_14.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test11: CTE that is defined but never used

WITH v AS (SELECT a, b FROM foo WHERE b < 5)
SELECT * FROM bar WHERE c = 8 ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_15.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test12: Full outer join query (generates a plan with CTEs)

SELECT * FROM foo FULL OUTER JOIN bar ON (foo.a = bar.c) ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_16.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test13: Query with grouping sets (generates a plan with CTEs)

SELECT a, count(*)
FROM foo GROUP BY GROUPING SETS ((),(a), (a,b)) ORDER BY 1;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_17.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test14: CTE with limit

WITH v AS (SELECT * FROM foo WHERE a < 10)
SELECT * FROM v v1, v v2 ORDER BY 1,2,3,4 LIMIT 1;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_18.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15a: CTE with a user-defined function [IMMUTABLE NO SQL]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
IMMUTABLE NO SQL
AS $$
BEGIN
RETURN a + 10;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_19.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15b: CTE with a user-defined function [IMMUTABLE CONTAINS SQL]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
IMMUTABLE CONTAINS SQL 
AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$;
WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_2.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test2: Single producer and multiple consumers

WITH v AS (SELECT a, b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_20.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15c: CTE with a user-defined function [STABLE NO SQL]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
STABLE NO SQL
AS $$
BEGIN
RETURN a + 10;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_21.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15d: CTE with a user-defined function [STABLE CONTAINS SQL]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
STABLE CONTAINS SQL
AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_24.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15g: CTE with a user-defined function [VOLATILE NO SQL]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
VOLATILE NO SQL
AS $$
BEGIN
RETURN a + 10;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_25.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15h: CTE with a user-defined function [VOLATILE CONTAINS SQL]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
VOLATILE CONTAINS SQL
AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$;


WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_3.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test3: Single producer and multiple consumers, with a predicate that can be pushed down one of the consumers

WITH v AS (SELECT a, b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a
AND v1.a < 10 ORDER BY 1,2;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_30.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo CASCADE;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar CASCADE;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test17a: CTE and views [View with a single CTE]

DROP VIEW IF EXISTS cte_view;
CREATE VIEW cte_view as 
(WITH cte(e)AS
(
	    SELECT d FROM bar
    INTERSECT 
    SELECT a FROM foo limit 10
)SELECT * FROM CTE);

\d cte_view

SELECT * FROM cte_view ORDER BY 1;
DROP TABLE IF EXISTS bar CASCADE;
DROP TABLE IF EXISTS foo CASCADE;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_31.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo CASCADE;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar CASCADE;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test17b: CTE and views [View with multiple CTE’s]

DROP VIEW IF EXISTS cte_view;
CREATE VIEW cte_view as 
( 
 WITH cte(e,f) AS (SELECT a,d FROM bar, foo WHERE foo.a = bar.d ),
      cte2(e,f) AS (SELECT e,d FROM bar, cte WHERE cte.e = bar.c )
SELECT cte2.e,cte.f FROM cte,cte2 where cte.e = cte2.e
);
\d cte_view

SELECT * FROM cte_view ORDER BY 1;

DROP TABLE IF EXISTS bar CASCADE;
DROP TABLE IF EXISTS foo CASCADE;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_32.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test18: CTE with WINDOW function

WITH CTE(a,b) AS
(SELECT a,d FROM foo, bar WHERE foo.a = bar.d),
CTE1(e,f) AS
( SELECT foo.a, rank() OVER (PARTITION BY foo.b ORDER BY CTE.a) FROM foo,CTE )
SELECT * FROM CTE1,CTE WHERE CTE.a = CTE1.f and CTE.a = 2 ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_33.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19a :CTE with set operations [UNION]

WITH ctemax(a,b) AS
(
SELECT a,b FROM foo
),
cte(e) AS
( SELECT b FROM ctemax
UNION SELECT a FROM foo)
SELECT e FROM cte ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_34.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19b :CTE with set operations [UNION ALL]

WITH Results_CTE AS (
    SELECT t2.a, ROW_NUMBER() OVER (ORDER BY b) AS RowNum FROM foo t2 LEFT JOIN bar ON bar.d = t2.b
UNION ALL 
    SELECT t1.b, ROW_NUMBER() OVER (ORDER BY a) AS RowNum FROM foo t1
LEFT JOIN bar ON bar.c = t1.a
 ) 
SELECT * FROM Results_CTE a INNER JOIN bar ON a.a = bar.d WHERE RowNum >= 0 AND RowNum <= 10 ORDER BY 1,2,3,4;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_35.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19c :CTE with set operations [INTERSECT]

WITH ctemax(a,b) AS
(
    SELECT a,b FROM foo 
),
    cte(e) AS
(SELECT b FROM ctemax
INTERSECT
SELECT a FROM foo)
SELECT e FROM cte ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_36.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19d :CTE with set operations [INTERSECT ALL]

WITH ctemax(a,b) AS( SELECT a,b FROM foo ),
    cte(e) AS(SELECT b FROM ctemax
              INTERSECT ALL
              SELECT a FROM foo)
SELECT e FROM cte ORDER BY 1;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_37.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19e :CTE with set operations [EXCEPT]

WITH ctemax(a,b) AS
(
SELECT a,b FROM foo
),
cte(e) AS
( SELECT b FROM ctemax
EXCEPT
SELECT a FROM foo)
SELECT e FROM cte ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_38.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19f :CTE with set operations [EXCEPT ALL]

WITH ctemax(a,b) AS
(
SELECT a,b FROM foo
),
cte(e) AS
( SELECT b FROM ctemax
EXCEPT ALL
SELECT a FROM foo)
SELECT e FROM cte ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_39.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS v;
CREATE TABLE v as SELECT generate_series(1,10)a;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test20: Common name for CTE and table 

WITH v AS (SELECT c, d FROM bar, v WHERE c = v.a ) SELECT v1.c, v1.d FROM v v1, v v2 WHERE v1.c = v2.c AND v1.d <10  ORDER BY 1;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_4.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test4: Multiple CTEs defined at the same level with no dependencies

WITH v AS (SELECT a, b FROM foo WHERE b < 5),
     w AS (SELECT c, d FROM bar WHERE c > 8)
SELECT v1.a, w1.c, w2.d
FROM v AS v1, v as v2, w AS w1, w AS w2
WHERE v1.a = v2.b
AND v1.a < w1.c
AND v1.b < w2.d ORDER BY 1,2,3;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_40.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test21a: Common name for CTEs and subquery alias

WITH v1 AS (SELECT a, b FROM foo WHERE a < 6), 
     v2 AS (SELECT * FROM v1 WHERE a < 3)
SELECT * 
FROM (
        SELECT * FROM v1 WHERE b < 5) v1,
       (SELECT * FROM v1) v2
WHERE v1.a =v2.b  ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_41.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test21b: Common name for table , CTE and sub-query alias

WITH foo AS (SELECT a, b FROM foo WHERE a < 5), 
     bar AS (SELECT c, d FROM bar WHERE c < 4)
SELECT * 
FROM (
        SELECT * FROM foo WHERE b < 5) foo,
       (SELECT * FROM bar) bar
WHERE foo.a =bar.d ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_42.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test22: Nested sub-query with same CTE name

SELECT avg(a3),b3
FROM
(
	WITH foo(b1,a1) AS (SELECT a,b FROM foo where a >= 1)
SELECT b3,a3 FROM
	(
 		WITH foo(b2,a2) AS ( SELECT a1,b1 FROM foo where a1 >= 1 )
  		SELECT b3,a3 FROM
 		(
			WITH foo(b3,a3) AS ( SELECT a2,b2 FROM foo where a2 >= 1 )
 			SELECT s1.b3,s1.a3 FROM foo s1,foo s2
  		) foo2
) foo1
) foo0 
GROUP BY b3 ORDER BY 1,2;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_43.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test23: CTE with Percentile function

WITH v AS (SELECT a, b FROM foo WHERE b < 5) select median(a) from v;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_44.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24a: CTE with CSQ [ANY]

WITH newfoo AS (SELECT * FROM foo WHERE foo.a = any (SELECT bar.d FROM bar WHERE bar.d = foo.a) ORDER BY 1,2)
SELECT foo.a,newfoo.b FROM foo,newfoo WHERE foo.a = newfoo.a ORDER BY 1;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_45.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24b: CTE with CSQ[EXISTS]  

WITH newfoo AS
	(
	     SELECT foo.* FROM foo WHERE EXISTS(SELECT bar.c FROM bar WHERE foo.b = bar.c) ORDER BY foo.b
)
SELECT
( SELECT max(CNT) FROM (SELECT count(*) CNT,nf1.b FROM newfoo nf1, newfoo nf2
WHERE nf1.a = nf2.a group by nf1.b) FOO
), * FROM newfoo ORDER BY 1,2,3;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_46.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24c: CTE with CSQ [NOT EXISTS] 

WITH newfoo AS (
SELECT b FROM foo WHERE NOT EXISTS (SELECT * FROM bar WHERE d=a) LIMIT 1
)
SELECT foo.a,newfoo.b FROM foo,newfoo WHERE foo.a = newfoo.b ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_47.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24d: CTE with CSQ [NOT IN] 

WITH newfoo AS (
SELECT foo.a FROM foo group by foo.a having min(foo.a) not in (SELECT bar.c FROM bar WHERE foo.a = bar.d) ORDER BY foo.a
) 
    SELECT foo.a,newfoo.a FROM foo,newfoo WHERE foo.a = newfoo.a ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_48.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test25a: CTE with different column List [Multiple CTE]

WITH CTE("A","B") as
	(SELECT c , d FROM bar WHERE c > 1),
CTE2("A","B") as
(SELECT a,b FROM foo WHERE a >6)
SELECT "A","B" from CTE2 order by "A";



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_49.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test25b: CTE with different column List [Multiple CTE with dependency]

WITH CTE("A","B") as
(SELECT c , d FROM bar WHERE c > 1),
CTE2("A","B")  AS (SELECT "A","B" FROM CTE WHERE "A">6)
SELECT "A","B" from CTE2 order by "A";


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_5.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test5: Multiple CTEs defined at the same level with dependencies

WITH v AS (SELECT a, b FROM foo WHERE b < 5),
     w AS (SELECT * FROM v WHERE a > 2)
SELECT v1.a, w1.b b1, w2.b b2
FROM v AS v1, v as v2, w AS w1, w AS w2
WHERE v1.a = v2.b
AND v1.a < w1.a
AND v1.b < w2.b ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_50.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test25c: Negative test - CTE with different column List , No quotes in column name

WITH CTE("A","B") as
(SELECT c , d FROM bar WHERE c > 1),
CTE2("A","B") as
(SELECT a,b FROM foo WHERE a >6)
SELECT A,B from CTE2 ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_51.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test25d: Negative Case - CTE with different column List, Ambiguous Column reference

WITH CTE(a,b) as
(SELECT c , d FROM bar WHERE c > 1)
SELECT a,b FROM CTE,foo WHERE CTE.a = foo.b ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_52.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test26a: CTE with CTAS

WITH CTE(c,d) as 
(
	SELECT a,b FROM foo WHERE a > 1
) SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d ORDER BY 1;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo as 
(
	WITH CTE(c,d) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d
);


SELECT * FROM newfoo ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_53.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test26b: CTE with CTAS, sub-query

WITH CTE(a,b) as 
(
        SELECT a,b FROM foo WHERE a > 1
) 
SELECT SUBFOO.c,CTE.a FROM 
(SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c) SUBFOO,
CTE WHERE SUBFOO.c = CTE.b ORDER BY 1;


DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo as 
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 

	SELECT SUBFOO.c,CTE.a FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) SUBFOO,CTE WHERE SUBFOO.c = CTE.b
);


SELECT * FROM newfoo ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_54.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test26c: CTE with CTAS , CTE and sub-query having same name

WITH CTE(a,b) as 

(
	SELECT a,b FROM foo WHERE a > 1
) 
SELECT CTE.* FROM 
( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c) CTE ORDER BY 1;


DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo as 
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 

	SELECT CTE.* FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) CTE
);


SELECT * FROM newfoo ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_55.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27a: DML with CTE [INSERT]

WITH CTE(c,d) as 
(
	SELECT a,b FROM foo WHERE a > 1
) SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d ORDER BY 1;

INSERT INTO newfoo
(
	WITH CTE(c,d) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d
);


SELECT * FROM newfoo ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_56.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27b: DML with CTE [INSERT with CTE and sub-query alias]

WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
SELECT SUBFOO.c,CTE.a FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b ORDER BY 1;


INSERT INTO newfoo
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
SELECT SUBFOO.c,CTE.a FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b
);

SELECT * FROM newfoo ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_57.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27c: DML with CTE [INSERT with CTE and sub-query alias having common name]

WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 

SELECT CTE.* FROM ( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
) CTE ORDER BY 1;

INSERT INTO newfoo
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 

	SELECT CTE.* FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) CTE
);

SELECT * FROM newfoo ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_6.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test6: CTE defined inside a subexpression (in the FROM clause)

WITH w AS (SELECT a, b from foo where b < 5)
SELECT *
FROM foo,
     (WITH v AS (SELECT c, d FROM bar, w WHERE c = w.a AND c < 2)
      SELECT v1.c, v1.d FROM v v1, v v2 WHERE v1.c = v2.c AND v1.d > 1
     ) x
WHERE foo.a = x.c ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_61.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
INSERT INTO newfoo SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27g: DML with CTE [ DELETE ]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(c,d) as
(
    SELECT a,b FROM foo WHERE a > 1
) 
SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d
ORDER BY 1;

DELETE FROM newfoo using(
WITH CTE(c,d) as
	(
	SELECT a,b FROM foo WHERE a > 1
	) 
	SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d
)sub;

SELECT * FROM newfoo;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_62.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
INSERT INTO newfoo SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27h: DML with CTE [ DELETE with CTE and sub-query alias]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(a,b) as 
(
    SELECT a,b FROM foo WHERE a > 1
) 
	SELECT SUBFOO.c,CTE.a FROM 
( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b ORDER BY 1;


DELETE FROM newfoo using(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
	SELECT SUBFOO.c,CTE.a FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b
)sub;
SELECT * FROM newfoo;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_63.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo (a int, b int);
INSERT INTO newfoo SELECT i as a, i+1 as b from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27i: DML with CTE [ DELETE with CTE and sub-query alias having common name]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(a,b) as 
(
	SELECT a,b FROM foo WHERE a > 1
) 
SELECT CTE.* FROM 
( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
) CTE ORDER BY 1;

DELETE FROM newfoo using(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
	SELECT CTE.* FROM 
		(
	SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) CTE
) sub;

SELECT * FROM newfoo;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_64.sql
-- ----------------------------------------------------------------------

-- start_ignore
set gp_enable_column_oriented_table=on;
DROP TABLE if exists foo_ao;
DROP TABLE if exists bar_co;

CREATE TABLE foo_ao(a int, b int) WITH ( appendonly = true);
CREATE TABLE bar_co(c int, d int) WITH ( appendonly = true, orientation = column);
INSERT INTO foo_ao SELECT i as a, i+1 as b FROM generate_series(1,10)i;

INSERT INTO bar_co SELECT i as c, i+1 as d FROM generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test28a: CTE with AO/CO tables
WITH v AS (SELECT a, b FROM foo_ao WHERE b < 5),
     w AS (SELECT c, d FROM bar_co WHERE c < 9)
SELECT v1.a, w1.c, w2.d
FROM v AS v1, v as v2, w AS w1, w AS w2
WHERE v1.a = v2.b
AND v1.a = w1.c
AND v1.b = w2.d ORDER BY 1;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_65.sql
-- ----------------------------------------------------------------------

-- start_ignore
set gp_enable_column_oriented_table=on;
DROP TABLE if exists foo_ao;
DROP TABLE if exists bar_co;
CREATE TABLE foo_ao(a int, b int) WITH ( appendonly = true);
CREATE TABLE bar_co(c int, d int) WITH ( appendonly = true, orientation = column);

INSERT INTO foo_ao SELECT i as a, i+1 as b FROM generate_series(1,10)i;
INSERT INTO bar_co SELECT i as c, i+1 as d FROM generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test28b: CTE with AO/CO tables[ Multiple CTE with dependency]
WITH v AS (SELECT a, b FROM foo_ao WHERE b < 5),
     w AS (SELECT * FROM v WHERE a < 2)
SELECT w.a, bar_co.d 
FROM w,bar_co
WHERE w.a = bar_co.c ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_66.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS v;

DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test29: Negative Test - Forward Reference

WITH v AS (SELECT c, d FROM bar, v WHERE c = v.a AND c < 2) SELECT v1.c, v1.d FROM v v1, v v2 WHERE v1.c = v2.c AND v1.d > 7;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_67.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test30: Negative Test - CTEs with same name

WITH v AS (SELECT a, b FROM foo WHERE b < 5),
     v AS (SELECT c, d FROM bar WHERE c < 2)
SELECT v1.a, v2.c 
FROM v AS v1, v as v2
WHERE v1.a =v2.c ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_68.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test31: Negative Test - Specified number of columns in WITH query exceeds the number of available columns

WITH v AS (SELECT a, b FROM foo WHERE b < 5),
     v AS (SELECT c, d FROM bar WHERE c < 2)
SELECT v1.a, v2.c 
FROM v AS v1, v as v2
WHERE v1.a =v2.c;
WITH CTE(a,b) AS
(SELECT * FROM FOO, BAR WHERE FOO.a = BAR.d)
SELECT * FROM CTE ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_69.sql
-- ----------------------------------------------------------------------

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test32:  Negative Test - Recursive WITH clause is not supported

WITH RECURSIVE t(n) AS (
SELECT 1
UNION ALL
SELECT n+1 FROM t
)
SELECT n FROM t;




-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_7.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test7a: CTE defined inside a subquery (in the WHERE clause)

SELECT *
FROM foo 
WHERE a = (WITH v as (SELECT * FROM bar WHERE c < 2)
		    SELECT max(v1.c) FROM v v1, v v2 WHERE v1.c = v2.c) ORDER BY 1;



-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_70.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd
-- @created 2013-02-01 12:00:00
-- @modified 2013-02-01 12:00:00
-- @tags cte HAWQ
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @description test8a: CTE defined in the HAVING clause

WITH w AS (SELECT a, b from foo where b < 5)
SELECT a, sum(b) FROM foo WHERE b > 1 GROUP BY a HAVING sum(b) < (SELECT d FROM bar, w WHERE c = w.a AND c > 2) ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_8.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test7b: CTE defined inside a subquery (in the WHERE clause)

SELECT *
FROM foo
WHERE a IN (WITH v as (SELECT * FROM bar WHERE c < 2) 
            SELECT v1.c FROM v v1, v v2 WHERE v1.c = v2.c) ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_9.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar AS SELECT i as c, i+1 as d from generate_series(1,10)i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test7c: CTE defined inside a subquery (in the WHERE clause)

SELECT *
FROM foo
WHERE a LIKE (WITH v as (SELECT * FROM bar WHERE c < 2) 
            SELECT v1.c FROM v v1, v v2 WHERE v1.c = v2.c) ORDER BY 1;


-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_26.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
DROP TABLE IF EXISTS foobar;
CREATE TABLE foobar (c int, d int);
INSERT INTO foobar select i, i+1 from generate_series(1,10) i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15i: CTE with a user-defined function [VOLATILE READS SQL DATA]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
VOLATILE READS SQL DATA
AS $$
DECLARE
    r int;
BEGIN
    SELECT d FROM foobar WHERE c = $1 LIMIT 1 INTO r;
    RETURN r;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: sql/cte_functest_27.sql
-- ----------------------------------------------------------------------

-- start_ignore
DROP TABLE IF EXISTS foo;
CREATE TABLE foo AS SELECT i as a, i+1 as b from generate_series(1,10)i;
DROP TABLE IF EXISTS foobar;
CREATE TABLE foobar (c int, d int);
INSERT INTO foobar select i, i+1 from generate_series(1,10) i;
-- end_ignore

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15j: CTE with a user-defined function [VOLATILE MODIFIES SQL DATA]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
VOLATILE MODIFIES SQL DATA
AS $$
BEGIN
UPDATE foobar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;



-- ----------------------------------------------------------------------
-- Test: sql_others/cte_functest_22_inlining_disabled.sql
-- ----------------------------------------------------------------------

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @gucs optimizer_cte_inlining = off
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15e: CTE with a user-defined function [STABLE READS SQL DATA]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
STABLE READS SQL DATA
AS $$
DECLARE
    r int;
BEGIN
    SELECT d FROM foobar WHERE c = $1 LIMIT 1 INTO r;
    RETURN r;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: sql_others/cte_functest_23_inlining_disabled.sql
-- ----------------------------------------------------------------------

-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @gucs optimizer_cte_inlining = off
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test15f: CTE with a user-defined function [STABLE MODIFIES SQL DATA]

CREATE OR REPLACE FUNCTION cte_func1(a int) RETURNS int 
LANGUAGE plpgsql
STABLE MODIFIES SQL DATA
AS $$
BEGIN
UPDATE foobar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$;

WITH v(a, b) AS (SELECT cte_func1(a), b FROM foo WHERE b < 5)
SELECT v1.a, v2.b
FROM v AS v1, v AS v2
WHERE v1.a < v2.a ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema cte_functest cascade;
-- end_ignore
