-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test26a: CTE with CTAS

WITH CTE(c,d) as 
(
	SELECT a,b FROM foo WHERE a > 1
) SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d ORDER BY 1;

DROP TABLE IF EXISTS newfoo;
CREATE TABLE newfoo as 
(
	WITH CTE(c,d) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d
);


SELECT * FROM newfoo ORDER BY 1;


