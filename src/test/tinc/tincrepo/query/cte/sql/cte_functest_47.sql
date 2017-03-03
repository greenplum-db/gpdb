-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24d: CTE with CSQ [NOT IN] 

WITH newfoo AS (
SELECT foo.a FROM foo group by foo.a having min(foo.a) not in (SELECT bar.c FROM bar WHERE foo.a = bar.d) ORDER BY foo.a
) 
    SELECT foo.a,newfoo.a FROM foo,newfoo WHERE foo.a = newfoo.a ORDER BY 1;


