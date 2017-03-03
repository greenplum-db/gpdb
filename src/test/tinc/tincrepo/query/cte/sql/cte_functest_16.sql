-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test13: Query with grouping sets (generates a plan with CTEs)

SELECT a, count(*)
FROM foo GROUP BY GROUPING SETS ((),(a), (a,b)) ORDER BY 1;
