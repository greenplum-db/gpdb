-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24c: CTE with CSQ [NOT EXISTS] 

WITH newfoo AS (
SELECT b FROM foo WHERE NOT EXISTS (SELECT * FROM bar WHERE d=a) LIMIT 1
)
SELECT foo.a,newfoo.b FROM foo,newfoo WHERE foo.a = newfoo.b ORDER BY 1;


