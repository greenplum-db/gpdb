-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test7a: CTE defined inside a subquery (in the WHERE clause)

SELECT *
FROM foo 
WHERE a = (WITH v as (SELECT * FROM bar WHERE c < 2)
		    SELECT max(v1.c) FROM v v1, v v2 WHERE v1.c = v2.c) ORDER BY 1;


