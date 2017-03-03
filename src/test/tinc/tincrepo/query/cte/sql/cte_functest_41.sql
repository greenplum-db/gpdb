-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test21b: Common name for table , CTE and sub-query alias

WITH foo AS (SELECT a, b FROM foo WHERE a < 5), 
     bar AS (SELECT c, d FROM bar WHERE c < 4)
SELECT * 
FROM (
        SELECT * FROM foo WHERE b < 5) foo,
       (SELECT * FROM bar) bar
WHERE foo.a =bar.d ORDER BY 1;

