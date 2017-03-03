-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test11: CTE that is defined but never used

WITH v AS (SELECT a, b FROM foo WHERE b < 5)
SELECT * FROM bar WHERE c = 8 ORDER BY 1;
