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


