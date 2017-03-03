-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test10: Multi-level nesting

WITH v as (WITH x as (
                       SELECT * FROM foo WHERE b < 5
                     ) 
           SELECT x1.a ,x1.b FROM x x1, x x2 
           WHERE x1.a = x2.a AND x1.a = (WITH y as (
						     SELECT * FROM x
                                                   ) 
					SELECT max(y1.b) FROM y y1, y y2 WHERE y1.a < y2.a)) 
SELECT * FROM v v1, v v2 WHERE v1.a < v2.b ORDER BY 1;
