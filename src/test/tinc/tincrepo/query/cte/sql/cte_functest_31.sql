-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte HAWQ 
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @description test17b: CTE and views [View with multiple CTE’s]

DROP VIEW IF EXISTS cte_view;
CREATE VIEW cte_view as 
( 
 WITH cte(e,f) AS (SELECT a,d FROM bar, foo WHERE foo.a = bar.d ),
      cte2(e,f) AS (SELECT e,d FROM bar, cte WHERE cte.e = bar.c )
SELECT cte2.e,cte.f FROM cte,cte2 where cte.e = cte2.e
);
\d cte_view

SELECT * FROM cte_view ORDER BY 1;

DROP TABLE IF EXISTS bar CASCADE;
DROP TABLE IF EXISTS foo CASCADE;


