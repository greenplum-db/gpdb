-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test20: Common name for CTE and table 

WITH v AS (SELECT c, d FROM bar, v WHERE c = v.a ) SELECT v1.c, v1.d FROM v v1, v v2 WHERE v1.c = v2.c AND v1.d <10  ORDER BY 1;



