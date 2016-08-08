-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology.
-- @description create and alter functions

\c db_test_bed

CREATE OR REPLACE FUNCTION add(integer, integer) RETURNS integer 
    AS 'select $1 + $2;' 
    LANGUAGE SQL 
    STABLE
    RETURNS NULL ON NULL INPUT; 
-- start_ignore
CREATE LANGUAGE plpgsql;
-- end_ignore
CREATE OR REPLACE FUNCTION increment(i integer) RETURNS 
integer AS $$ 
        BEGIN 
                RETURN i + 1; 
        END; 
$$ LANGUAGE plpgsql
VOLATILE; 


CREATE FUNCTION dup(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE STRICT;

CREATE FUNCTION dup1(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  EXTERNAL  SECURITY INVOKER  ;

CREATE FUNCTION dup2(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  EXTERNAL  SECURITY DEFINER  ;

CREATE FUNCTION dup3(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  SECURITY INVOKER  ;

CREATE FUNCTION dup4(in int, out f1 int, out f2 text) 
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$ 
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  SECURITY DEFINER  ;

CREATE ROLE func_role;
CREATE SCHEMA func_schema;

ALTER FUNCTION add(integer, integer) OWNER TO func_role;
ALTER FUNCTION add(integer, integer)  SET SCHEMA func_schema;
set search_path to func_schema;
ALTER FUNCTION add(integer, integer)  SET SCHEMA public;
set search_path to public;
ALTER FUNCTION add(integer, integer)   RENAME TO new_add;
ALTER FUNCTION new_add(integer, integer)   RENAME TO add;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) CALLED ON NULL INPUT VOLATILE EXTERNAL SECURITY DEFINER;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) IMMUTABLE STRICT SECURITY INVOKER;
ALTER FUNCTION dup(in int, out f1 int, out f2 text) IMMUTABLE STRICT SECURITY DEFINER;
