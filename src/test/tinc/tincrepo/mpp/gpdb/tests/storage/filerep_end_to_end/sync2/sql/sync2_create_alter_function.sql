-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--  Create funsync2ion, Alter funsync2ion

CREATE FUNCTION sync2_func1(in int, out f1 int, out f2 text)
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  EXTERNAL  SECURITY INVOKER  ;

CREATE FUNCTION sync2_func2(in int, out f1 int, out f2 text)
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  EXTERNAL  SECURITY DEFINER  ;

ALTER FUNCTION sync1_func5(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION ck_sync1_func4(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION ct_func3(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION resync_func2(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION sync2_func1(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
