-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test25d: Negative Case - CTE with different column List, Ambiguous Column reference

WITH CTE(a,b) as
(SELECT c , d FROM bar WHERE c > 1)
SELECT a,b FROM CTE,foo WHERE CTE.a = foo.b ORDER BY 1;


