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


