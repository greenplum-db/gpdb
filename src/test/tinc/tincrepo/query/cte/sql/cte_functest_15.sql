-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test12: Full outer join query (generates a plan with CTEs)

SELECT * FROM foo FULL OUTER JOIN bar ON (foo.a = bar.c) ORDER BY 1;
