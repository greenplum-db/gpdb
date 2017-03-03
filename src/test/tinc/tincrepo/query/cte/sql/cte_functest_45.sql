-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test24b: CTE with CSQ[EXISTS]  

WITH newfoo AS
	(
	     SELECT foo.* FROM foo WHERE EXISTS(SELECT bar.c FROM bar WHERE foo.b = bar.c) ORDER BY foo.b
)
SELECT
( SELECT max(CNT) FROM (SELECT count(*) CNT,nf1.b FROM newfoo nf1, newfoo nf2
WHERE nf1.a = nf2.a group by nf1.b) FOO
), * FROM newfoo ORDER BY 1,2,3;


