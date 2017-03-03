-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test19b :CTE with set operations [UNION ALL]

WITH Results_CTE AS (
    SELECT t2.a, ROW_NUMBER() OVER (ORDER BY b) AS RowNum FROM foo t2 LEFT JOIN bar ON bar.d = t2.b
UNION ALL 
    SELECT t1.b, ROW_NUMBER() OVER (ORDER BY a) AS RowNum FROM foo t1
LEFT JOIN bar ON bar.c = t1.a
 ) 
SELECT * FROM Results_CTE a INNER JOIN bar ON a.a = bar.d WHERE RowNum >= 0 AND RowNum <= 10 ORDER BY 1,2,3,4;


