-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip OPT-2497
-- @description test27e: DML with CTE [ UPDATE with CTE and sub-query alias]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(a,b) as 
(
    SELECT a,b FROM foo WHERE a > 1
) 
SELECT SUBFOO.c,CTE.a FROM 
( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c ORDER BY 1
) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b ORDER BY 1;

UPDATE newfoo SET b =sub.c FROM 
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
	SELECT SUBFOO.c,CTE.a FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b
) sub;

SELECT * FROM newfoo ORDER BY 1;

