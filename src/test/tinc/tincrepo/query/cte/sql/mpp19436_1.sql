-- @author prabhd
-- @created 2012-02-01 12:00:00
-- @modified 2013-02-01 12:00:00
-- @tags cte bfv MPP-19436
-- @db_name world_db
-- @product_version gpdb: [4.2.6.1-],hawq: [1.1.0.2-]
-- @description MPP-19436

WITH t AS
(
 SELECT e.*,f.*
 FROM
    (
      SELECT * FROM foo WHERE a < 10 
    ) e
 LEFT OUTER JOIN
    (
       SELECT * FROM bar WHERE c < 10 
    ) f

  ON e.a = f.d ) 
SELECT t.a,t.d, count(*) over () AS window
FROM t 
GROUP BY t.a,t.d ORDER BY t.a,t.d LIMIT 2;
