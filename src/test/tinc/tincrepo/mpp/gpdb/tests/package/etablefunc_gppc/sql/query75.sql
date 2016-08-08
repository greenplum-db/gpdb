    --start_ignore
    -- Drop dyr table function first
    DROP FUNCTION IF EXISTS project(anytable, integer);
    DROP FUNCTION IF EXISTS project_desc(internal);
    --end_ignore

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

    -- Check callback function project_desc is removed from pg_proc_callback
            select * from pg_proc_callback 
            where profnoid='project'::regproc 
            and procallback='project_desc'::regproc;

   -- Verify can drop describe (callback) function 
   -- when no other function is using it
