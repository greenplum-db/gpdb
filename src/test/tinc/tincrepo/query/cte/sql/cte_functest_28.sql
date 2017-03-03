-- @author prabhd 
-- @created 2013-02-01 12:00:00 
-- @modified 2013-02-01 12:00:00 
-- @tags cte
-- @product_version gpdb: [4.3-],hawq: [1.1-]
-- @db_name world_db
-- @skip OPT-3035
-- @description test16a: CTE within a user-defined function

CREATE OR REPLACE FUNCTION cte_func3()
RETURNS SETOF int 
RETURNS NULL ON NULL INPUT
AS $$
    id = []
    rv = plpy.execute("SELECT * FROM (WITH v AS (SELECT a, b FROM foo WHERE b < 9)\
SELECT v1.a FROM v AS v1, v as v2 WHERE v1.a = v2.b)OUTERFOO ORDER BY 1", 5)
    for i in range(0,5):
        val = rv[i]["a"]
        id.append(val)
    return id
$$ LANGUAGE plpythonu READS SQL DATA;

WITH v(a, b) AS (SELECT a,b FROM foo WHERE b < 5)
SELECT * from v where b in ( select * from cte_func3()) ORDER BY 1;


