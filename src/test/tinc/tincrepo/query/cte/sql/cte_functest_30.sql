-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test17a: CTE and views [View with a single CTE]

DROP VIEW IF EXISTS cte_view;
CREATE VIEW cte_view as 
(WITH cte(e)AS
(
	    SELECT d FROM bar
    INTERSECT 
    SELECT a FROM foo limit 10
)SELECT * FROM CTE);

\d cte_view

SELECT * FROM cte_view ORDER BY 1;
DROP TABLE IF EXISTS bar CASCADE;
DROP TABLE IF EXISTS foo CASCADE;


