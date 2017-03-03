-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip OPT-2497
-- @description test27f: DML with CTE [ UPDATE with CTE and sub-query alias having common name]

SELECT * FROM newfoo ORDER BY 1;

WITH CTE(a,b) as 
(
    SELECT a,b FROM foo WHERE a > 1
)
SELECT CTE.* FROM
( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.d ORDER BY 1
) CTE ORDER BY 1;


UPDATE newfoo SET b =sub.c FROM 
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	)
	SELECT CTE.* FROM
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.d ORDER BY 1
		) CTE
) sub;

SELECT * FROM newfoo ORDER BY 1;



