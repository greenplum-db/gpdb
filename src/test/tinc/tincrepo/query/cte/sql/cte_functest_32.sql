-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test18: CTE with WINDOW function

WITH CTE(a,b) AS
(SELECT a,d FROM foo, bar WHERE foo.a = bar.d),
CTE1(e,f) AS
( SELECT foo.a, rank() OVER (PARTITION BY foo.b ORDER BY CTE.a) FROM foo,CTE )
SELECT * FROM CTE1,CTE WHERE CTE.a = CTE1.f and CTE.a = 2 ORDER BY 1;


