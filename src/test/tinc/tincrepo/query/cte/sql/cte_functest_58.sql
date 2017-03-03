-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip OPT-2497
-- @description test27d: DML with CTE [UPDATE]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(c,d) as
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d ORDER BY 1;


UPDATE newfoo SET b =sub.c FROM 
(
	WITH CTE(c,d) as
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
	SELECT CTE.c,bar.d FROM  CTE,bar where CTE.c = bar.d ORDER BY 1
) sub;

SELECT * FROM newfoo ORDER BY 1;

