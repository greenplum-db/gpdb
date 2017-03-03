-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test31: Negative Test - Specified number of columns in WITH query exceeds the number of available columns

WITH v AS (SELECT a, b FROM foo WHERE b < 5),
     v AS (SELECT c, d FROM bar WHERE c < 2)
SELECT v1.a, v2.c 
FROM v AS v1, v as v2
WHERE v1.a =v2.c;
WITH CTE(a,b) AS
(SELECT * FROM FOO, BAR WHERE FOO.a = BAR.d)
SELECT * FROM CTE ORDER BY 1;


