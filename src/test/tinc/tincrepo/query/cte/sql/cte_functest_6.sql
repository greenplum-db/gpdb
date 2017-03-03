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


