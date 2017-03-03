-- @author prabhd
-- @created 2012-02-01 12:00:00
-- @modified 2013-02-01 12:00:00
-- @tags cte bfv MPP-19436
-- @db_name world_db
-- @product_version gpdb: [4.2.6.1-],hawq: [1.1.0.2-]
-- @description MPP-19436
WITH t(a,b,d) AS
(
  SELECT foo.a,foo.b,bar.d FROM foo,bar WHERE foo.a = bar.d
)
SELECT t.b,avg(t.a), rank() OVER (PARTITION BY t.a ORDER BY t.a) FROM foo,t GROUP BY foo.a,foo.b,t.b,t.a ORDER BY 1,2,3 LIMIT 5;

