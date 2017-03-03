-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip OPT-3035
-- @description test16b: CTE within a user-defined function

CREATE OR REPLACE FUNCTION cte_func2()
RETURNS int 
as $$
Declare
    rcount INTEGER;
Begin
RETURN (SELECT COUNT(*) FROM (WITH v AS (SELECT a, b FROM foo WHERE b < 9),
w AS (SELECT * FROM v WHERE a < 5)
SELECT v1.a, w1.b b1, w2.b b2
FROM v AS v1, v as v2, w AS w1, w AS w2
WHERE v1.a = v2.b)foo);
End;
$$ language plpgsql READS SQL DATA;


WITH v(a, b) AS (SELECT cte_func2() as a, b FROM foo WHERE b < 5)
SELECT * from v ORDER BY 1;




