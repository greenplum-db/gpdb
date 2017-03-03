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


