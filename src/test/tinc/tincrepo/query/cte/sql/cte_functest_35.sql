-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19c :CTE with set operations [INTERSECT]

WITH ctemax(a,b) AS
(
    SELECT a,b FROM foo 
),
    cte(e) AS
(SELECT b FROM ctemax
INTERSECT
SELECT a FROM foo)
SELECT e FROM cte ORDER BY 1;


