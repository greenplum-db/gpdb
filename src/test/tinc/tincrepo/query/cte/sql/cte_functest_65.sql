-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test28b: CTE with AO/CO tables[ Multiple CTE with dependency]
WITH v AS (SELECT a, b FROM foo_ao WHERE b < 5),
     w AS (SELECT * FROM v WHERE a < 2)
SELECT w.a, bar_co.d 
FROM w,bar_co
WHERE w.a = bar_co.c ORDER BY 1;


