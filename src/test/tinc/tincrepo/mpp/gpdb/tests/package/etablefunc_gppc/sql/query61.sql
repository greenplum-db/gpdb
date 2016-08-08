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

    -- Using order by and scatter by inside ETF, with order by outside ETF
    SELECT * FROM project( 
        TABLE( SELECT * FROM history order by id scatter by id), 1) 
    order by 1;

    -- Using distinct outside ETF, scatter by multiple columns inside ETF
    SELECT distinct id FROM project( 
        TABLE( SELECT id FROM history order by id scatter by id, time), 1) 
    order by 1 desc;

    -- Using distinct filter inside ETF, and filter outside ETF
    SELECT time FROM project( 
        TABLE( SELECT distinct * FROM history scatter by id), 2) 
    where time <'2011-08-20' order by 1;

