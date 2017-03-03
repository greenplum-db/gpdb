-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27h: DML with CTE [ DELETE with CTE and sub-query alias]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(a,b) as 
(
    SELECT a,b FROM foo WHERE a > 1
) 
	SELECT SUBFOO.c,CTE.a FROM 
( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b ORDER BY 1;


DELETE FROM newfoo using(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 
	SELECT SUBFOO.c,CTE.a FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) SUBFOO,CTE  WHERE SUBFOO.c = CTE.b
)sub;
SELECT * FROM newfoo;


