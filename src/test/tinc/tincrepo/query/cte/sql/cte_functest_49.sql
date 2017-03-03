-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test25b: CTE with different column List [Multiple CTE with dependency]

WITH CTE("A","B") as
(SELECT c , d FROM bar WHERE c > 1),
CTE2("A","B")  AS (SELECT "A","B" FROM CTE WHERE "A">6)
SELECT "A","B" from CTE2 order by "A";

