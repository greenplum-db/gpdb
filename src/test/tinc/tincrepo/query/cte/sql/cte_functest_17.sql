-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test14: CTE with limit

WITH v AS (SELECT * FROM foo WHERE a < 10)
SELECT * FROM v v1, v v2 ORDER BY 1,2,3,4 LIMIT 1;



