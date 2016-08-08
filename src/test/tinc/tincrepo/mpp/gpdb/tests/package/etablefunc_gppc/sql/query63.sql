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

   -- Using constant input to ETF
    SELECT * FROM project( TABLE( SELECT 'col_1','col_2','col_3' ), 2);

