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

    -- ETF recursive call
    SELECT * FROM project( TABLE( SELECT * FROM (
        SELECT * FROM project (
            TABLE (SELECT * FROM history where time is not null 
            order by id scatter by time), 2) as project_alias1
        ) as project_alias2 
        order by time scatter by time), 1) 
    order by 1;

