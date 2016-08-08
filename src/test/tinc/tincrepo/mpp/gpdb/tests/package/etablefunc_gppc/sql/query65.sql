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

    -- Valid operations on results
    -- Using expression in scatter by and projected column
    SELECT id+1 FROM project( TABLE( SELECT * FROM history where id >2 scatter by id+1), 1) order by 1;

    -- Avg function
    SELECT avg(id) FROM project( TABLE( SELECT * FROM history ), 1);

    -- extract function, which takes timestamp type as input
    SELECT extract(day from "time") FROM project( TABLE( SELECT * FROM history where time >'2011-08-21'), 2) order by 1;

