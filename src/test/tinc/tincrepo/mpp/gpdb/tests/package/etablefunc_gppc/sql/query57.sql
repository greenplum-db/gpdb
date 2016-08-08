-- Verify pg_proc catalog table for specific columns:
-- # prorettype
-- # proargtypes
-- # proallargtypes
-- # proargmodes
-- # proargnames

\x

    -- create describe (callback) function
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION project_desc(internal)
    RETURNS internal
    AS '$libdir/tabfunc_gppc_demo', 'project_describe'
    LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION project(anytable, integer)
    RETURNS setof record
    AS '$libdir/tabfunc_gppc_demo', 'project'
    LANGUAGE C
    WITH (describe = project_desc);

SELECT prorettype, proargtypes, proallargtypes
       proargmodes, proargnames
FROM pg_proc 
WHERE oid='project_desc'::regproc
    or oid='project'::regproc
ORDER BY oid;
