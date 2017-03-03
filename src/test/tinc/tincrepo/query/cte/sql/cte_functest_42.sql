-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test22: Nested sub-query with same CTE name

SELECT avg(a3),b3
FROM
(
	WITH foo(b1,a1) AS (SELECT a,b FROM foo where a >= 1)
SELECT b3,a3 FROM
	(
 		WITH foo(b2,a2) AS ( SELECT a1,b1 FROM foo where a1 >= 1 )
  		SELECT b3,a3 FROM
 		(
			WITH foo(b3,a3) AS ( SELECT a2,b2 FROM foo where a2 >= 1 )
 			SELECT s1.b3,s1.a3 FROM foo s1,foo s2
  		) foo2
) foo1
) foo0 
GROUP BY b3 ORDER BY 1,2;


