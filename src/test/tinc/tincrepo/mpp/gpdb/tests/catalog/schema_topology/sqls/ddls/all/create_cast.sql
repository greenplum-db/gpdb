-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description creates cast

\c db_test_bed
CREATE OR REPLACE FUNCTION int4(boolean)
  RETURNS int4 AS
$BODY$

SELECT CASE WHEN $1 THEN 1 ELSE 0 END;

$BODY$
  LANGUAGE 'sql' IMMUTABLE;

-- start_ignore
CREATE CAST (boolean AS int4) WITH FUNCTION int4(boolean) AS ASSIGNMENT;

CREATE CAST (varchar AS text) WITHOUT FUNCTION AS IMPLICIT;
-- end_ignore
