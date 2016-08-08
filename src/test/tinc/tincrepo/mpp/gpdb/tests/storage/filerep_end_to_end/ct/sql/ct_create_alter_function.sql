-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--  Create function, Alter function

CREATE FUNCTION ct_func1(in int, out f1 int, out f2 text)
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  EXTERNAL  SECURITY INVOKER  ;

CREATE FUNCTION ct_func2(in int, out f1 int, out f2 text)
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  EXTERNAL  SECURITY DEFINER  ;

CREATE FUNCTION ct_func3(in int, out f1 int, out f2 text)
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  SECURITY INVOKER  ;

CREATE FUNCTION ct_func4(in int, out f1 int, out f2 text)
    AS $$ SELECT $1, CAST($1 AS text) || ' is text' $$
    LANGUAGE SQL IMMUTABLE CALLED ON NULL INPUT  SECURITY DEFINER  ;


ALTER FUNCTION sync1_func3(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION ck_sync1_func2(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
ALTER FUNCTION ct_func1(in int, out f1 int, out f2 text) RETURNS NULL ON NULL INPUT STABLE EXTERNAL SECURITY INVOKER;
