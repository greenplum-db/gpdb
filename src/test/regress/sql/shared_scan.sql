--
-- Queries that lead to hanging (not dead lock) when we don't handle synchronization properly in shared scan
-- Queries that lead to wrong result when we don't finish executing the subtree below the shared scan being squelched.
--

CREATE SCHEMA shared_scan;

SET search_path = shared_scan;

CREATE TABLE foo (a int, b int);
CREATE TABLE bar (c int, d int);
CREATE TABLE jazz(e int, f int);

INSERT INTO foo values (1, 2);
INSERT INTO bar SELECT i, i from generate_series(1, 100)i;
INSERT INTO jazz VALUES (2, 2), (3, 3);

ANALYZE foo;
ANALYZE bar;
ANALYZE jazz;

SELECT $query$
SELECT * FROM
        (
        WITH cte AS (SELECT * FROM foo)
        SELECT * FROM (SELECT * FROM cte UNION ALL SELECT * FROM cte)
        AS X
        JOIN bar ON b = c
        ) AS XY
        JOIN jazz on c = e AND b = f;
$query$ AS qry \gset

-- We are very particular about this plan shape and data distribution with ORCA:
-- 1. `jazz` has to be the inner table of the outer HASH JOIN, so that on a
-- segment which has zero tuples in `jazz`, the Sequence node that contains the
-- Shared Scan will be squelched on that segment. If `jazz` is not on the inner
-- side, the above mentioned "hang" scenario will not be covered.
-- 2. The Shared Scan producer has to be on a different slice from consumers,
-- and some tuples coming out of the Share Scan producer on one segments are
-- redistributed to a different segment over Motion. If not, the above mentioned
-- "wrong result" scenario will not be covered.
EXPLAIN (COSTS OFF)
:qry ;

SET statement_timeout = '15s';

:qry ;

RESET statement_timeout;

SELECT *,
        (
        WITH cte AS (SELECT * FROM jazz WHERE jazz.e = bar.c)
        SELECT 1 FROM cte c1, cte c2
        )
        FROM bar;

CREATE TABLE t1 (a int, b int);
CREATE TABLE t2 (a int);

-- ORCA plan contains a Shared Scan producer with a unsorted Motion below it
EXPLAIN (COSTS OFF)
WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 LIMIT 10) SELECT a, 1, 1 FROM cte JOIN t2 USING (a);
-- This functions returns one more column than expected.
CREATE OR REPLACE FUNCTION col_mismatch_func1() RETURNS TABLE (field1 int, field2 int)
LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
   v_qry text;
BEGIN
   v_qry := 'WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 LIMIT 10) SELECT a, 1 , 1 FROM cte JOIN t2 USING (a)';
  RETURN QUERY EXECUTE v_qry;
END
$$;

-- This should only ERROR and should not SIGSEGV
SELECT col_mismatch_func1();

-- ORCA plan contains a Shared Scan producer with a sorted Motion below it
EXPLAIN (COSTS OFF)
WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 ORDER BY b LIMIT 10) SELECT a, 1, 1 FROM cte JOIN t2 USING (a);
--- This functions returns one more column than expected.
CREATE OR REPLACE FUNCTION col_mismatch_func2() RETURNS TABLE (field1 int, field2 int)
    LANGUAGE 'plpgsql' VOLATILE STRICT AS
$$
DECLARE
    v_qry text;
BEGIN
    v_qry := 'WITH cte AS (SELECT * FROM t1 WHERE random() < 0.1 ORDER BY b LIMIT 10) SELECT a, 1 , 1 FROM cte JOIN t2 USING (a)';
    RETURN QUERY EXECUTE v_qry;
END
$$;

-- This should only ERROR and should not SIGSEGV
SELECT col_mismatch_func2();
