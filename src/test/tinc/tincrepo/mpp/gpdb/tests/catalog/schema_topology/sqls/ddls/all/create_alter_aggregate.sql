-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description This ddl makes use of create alter and aggregate statements

\c db_test_bed
CREATE FUNCTION scube_accum(numeric, numeric) RETURNS 
numeric 
    AS 'select $1 + $2 * $2 * $2' 
    LANGUAGE SQL 
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 

CREATE FUNCTION pre_accum(numeric, numeric) RETURNS 
numeric 
    AS 'select $1 + $2 * $2 * $2 * $2' 
    LANGUAGE SQL 
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE FUNCTION final_accum(numeric) RETURNS 
numeric 
    AS 'select $1 + $1 * $1' 
    LANGUAGE SQL 
    IMMUTABLE 
    RETURNS NULL ON NULL INPUT; 


CREATE AGGREGATE scube(numeric) ( 
    SFUNC = scube_accum, 
    STYPE = numeric, 
         PREFUNC =pre_accum,
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 


CREATE AGGREGATE old_scube ( 
	BASETYPE = numeric,
    SFUNC = scube_accum, 
    STYPE = numeric, 
       FINALFUNC =final_accum,
    INITCOND = 0 ,
          SORTOP = >); 

CREATE ROLE agg_owner;
CREATE SCHEMA agg_schema;

ALTER AGGREGATE scube(numeric) RENAME TO new_scube;
ALTER AGGREGATE new_scube(numeric) RENAME TO scube;
ALTER AGGREGATE scube(numeric) OWNER TO agg_owner;
ALTER AGGREGATE scube(numeric) SET SCHEMA agg_schema;
