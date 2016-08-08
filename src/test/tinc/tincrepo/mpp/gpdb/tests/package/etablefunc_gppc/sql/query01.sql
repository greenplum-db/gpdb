    -- Create Enhanced Table Function (ETF) using input syntax: 
    -- CREATE FUNCTION tabfunc(t anytable) RETURNS ...

    -- MPP-16614, the SELECT query would fail
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS SETOF outtable
    AS '$libdir/tabfunc_gppc_demo', 'mytransform'
    LANGUAGE C;

    \df transform

    SELECT * FROM transform( 
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
    ) order by b;

    -- The 1st workaround of MPP-16614
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS SETOF RECORD
    AS '$libdir/tabfunc_gppc_demo', 'mytransform'
    LANGUAGE C;

    SELECT * FROM transform( 
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
    )  AS t (a text, b int) order by b;

    -- The 2nd workaround of MPP-16614
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS TABLE (a text, b int)
    AS '$libdir/tabfunc_gppc_demo', 'mytransform'
    LANGUAGE C;

    SELECT * FROM transform(
        TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id)
    ) order by b;

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

    -- Check callback function project_desc is registerred in pg_proc_callback
    select * from pg_proc_callback 
    where profnoid='project'::regproc 
    and procallback='project_desc'::regproc;

