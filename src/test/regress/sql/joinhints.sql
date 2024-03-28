-- Test Optimizer Join Hints Feature
--
-- Purpose: Test that join hints may be used to coerce certain plan shapes

LOAD 'pg_hint_plan';

DROP SCHEMA IF EXISTS joinhints CASCADE;

CREATE SCHEMA joinhints;
SET search_path=joinhints;
SET optimizer_trace_fallback=on;

-- Setup tables
CREATE TABLE t1(a int, b int);
CREATE TABLE t2(a int, b int);
CREATE TABLE t3(a int, b int);
CREATE TABLE t4(a int, b int);
CREATE TABLE t5(a int, b int);

-- Test that join order hint for every tree shape is applied.
--
-- These check that every possible order on 3 relations. There are 12 possible
-- orders:
--
-- T1 T2 T3   =>   (T1 T2) T3,  T1 (T2 T3)
-- T1 T3 T2   =>   (T1 T3) T2,  T1 (T3 T2)
-- T2 T1 T3   =>   (T2 T1) T3,  T2 (T1 T3)
-- T2 T3 T1   =>   (T2 T3) T1,  T2 (T3 T1)
-- T3 T1 T2   =>   (T3 T1) T2,  T3 (T1 T2)
-- T3 T2 T1   =>   (T3 T2) T1,  T3 (T2 T1)

/*+
    Leading((t1 (t2 t3)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading((t1 (t3 t2)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(((t2 t3) t1))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(((t3 t2) t1))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(((t1 t3) t2))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(((t3 t1) t2))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading((t2 (t1 t3)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading((t2 (t3 t1)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(((t1 t2) t3))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(((t2 t1) t3))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading((t3 (t1 t2)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading((t3 (t2 t1)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;


-- Test that join order hint may be applied over non-leaf nodes
--
-- These check that LIMIT, GROUP BY, or CTE queries may be coerced by a plan
-- hint to produce a specific join order.

/*+
    Leading((t2 (t1 t3)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, (SELECT * FROM t3 LIMIT 42) AS q;

/*+
    Leading(((t5 (t4 t3)) (t1 t2)))
 */
EXPLAIN (costs off) SELECT * FROM (SELECT t1.a FROM t1, t2 LIMIT 42) AS q, t3, t4, t5;

/*+
  Leading((t1 (t2 t5)))
*/
EXPLAIN (costs off) SELECT * FROM (SELECT * FROM t1, t2, t5 LIMIT 42) q, t4, t3;

/*+
    Leading((((t1 t2) t3) t4))
 */
EXPLAIN (COSTS OFF) SELECT * FROM (SELECT t1.a, t2.a FROM t1, t2, t3 GROUP BY t1.a, t2.a) AS q, t4;

/*+
    Leading(((((g2_t1 g2_t2) g2_t3) t4) (g1_t1 (g1_t3 g1_t2))))
 */
EXPLAIN (COSTS OFF) SELECT * FROM (SELECT g1_t1.a, g1_t2.a FROM t1 AS g1_t1, t2 AS g1_t2, t3 AS g1_t3 GROUP BY g1_t1.a, g1_t2.a) AS q1,
                                  (SELECT g2_t1.a, g2_t2.a FROM t1 AS g2_t1, t2 AS g2_t2, t3 AS g2_t3 GROUP BY g2_t1.a, g2_t2.a) AS q2, t4, t5;

/*+
    Leading(((t3 t2) t1))
    Leading((t5 t4))
 */
EXPLAIN (COSTS OFF)
WITH cte AS
(
    SELECT * FROM t1, t2, t3
)
SELECT * FROM cte, t4, t5;


-- Test that bad join order hint is *not* applied
--
-- These check that invalid plans are not produced.

-- No plan joins (t2 t3) becase the query has LIMIT on (t1 t2) which must be
-- applied after joining (t1 t2)
/*+
    Leading((t1 t3))
 */
EXPLAIN (costs off) SELECT * FROM (SELECT t1.a FROM t1, t2 LIMIT 42) AS q, t3, t4, t5;

-- No plan joins (t2 t3) because the GROUP BY is on (t1 t2)
/*+
    Leading((t2 t3))
 */
EXPLAIN (COSTS OFF) SELECT * FROM (SELECT t1.a, t2.a FROM t1, t2 GROUP BY t1.a, t2.a) AS q, t3;

-- syntax error: extra parens
/*+
    Leading(((t1 t2)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2;

-- syntax error: cannot mix directed and non-directed hint
/*+
    Leading((t1 (t2 t3 t4)))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3, t4;


/*+
    Leading((((t5 t4) (t1 t3)) t2))
 */
EXPLAIN (costs off) SELECT t1.a, t2.a FROM t1, t2, t3, t4, t5;

/*+
    Leading(((t5 t4) (t1 t3)))
 */
EXPLAIN (costs off) SELECT t1.a, t2.a FROM t1, t2, t3, t4, t5;

/*+
    Leading((t2 t1))
 */
EXPLAIN (costs off) SELECT t1.a, t2.a FROM t1 JOIN t2 ON t1.a=t2.a JOIN t3 ON t3.a=t1.a, t4, t5;

/*+
    Leading((t5 (((t4 t3) t2) t1)))
 */
EXPLAIN (costs off) SELECT t1.a, t2.a FROM t1 JOIN t2 ON t1.a=t2.a JOIN t3 ON t3.a=t1.a, t4, t5;

/*+
    Leading((t5 (((t4 t3) t2) t1)))
 */
EXPLAIN (costs off) SELECT t1.a, t2.a FROM t1 JOIN t2 ON t1.a=t2.a JOIN t3 ON t3.a=t1.a+1 AND t3.a>42, t4, t5;


-- Test that multiple join order hint can be applied
--
-- Following queries produce multiple NAry join operators where different join
-- order hints may be applied.

/*+
  Leading((t3 t4))
  Leading((t1 (t2 t5)))
*/
EXPLAIN (costs off) SELECT * FROM t4, t3, (SELECT * FROM t1, t2, t5 LIMIT 42) q;

-- mixes directioned and directioned-less hint syntax
/*+
  Leading((t3 t4))
  Leading(t1 t2 t5)
*/
EXPLAIN (costs off) SELECT * FROM t4, t3, (SELECT * FROM t1, t2, t5 LIMIT 42) q;

/*+
  Leading((t3 t4))
  Leading((t1 (t2 t5)))
*/
EXPLAIN (costs off) SELECT * FROM (SELECT * FROM t4, t3 LIMIT 1) p, (SELECT * FROM t1, t2, t5 LIMIT 42) q;


-- Test that directioned-less join order hints can be applied
--
-- Following queries use directioned-less syntax. Example:
--
--     "Leading(t1 t2 ... tn)"
--
-- Above example specifies that t1 JOIN t2 happens *before* JOIN tn. But t1 can
-- be on the inner or outer side of the join. In contrast "Leading((t1 t2))
-- requires t1 to be on the outer side of the join.

/*+
    Leading(t1 t2 t3)
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

/*+
    Leading(t3 t2 t1)
 */
EXPLAIN (costs off) SELECT * FROM t1, t2, t3;

-- Test join order hints on non-inner join queries

/*+
    Leading(((t3 t2) t1))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2 LEFT JOIN t3 ON t2.a=t3.a;

/*+
    Leading(((t3 t2) t1))
 */
EXPLAIN (costs off) SELECT * FROM t1, t2 RIGHT JOIN t3 ON t2.a=t3.a;
