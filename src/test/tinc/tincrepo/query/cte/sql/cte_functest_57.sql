-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test27c: DML with CTE [INSERT with CTE and sub-query alias having common name]

WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 

SELECT CTE.* FROM ( SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
) CTE ORDER BY 1;

INSERT INTO newfoo
(
	WITH CTE(a,b) as 
	(
		SELECT a,b FROM foo WHERE a > 1
	) 

	SELECT CTE.* FROM 
		(
			SELECT CTE.a,bar.c FROM CTE,bar WHERE CTE.a = bar.c
		) CTE
);

SELECT * FROM newfoo ORDER BY 1;


