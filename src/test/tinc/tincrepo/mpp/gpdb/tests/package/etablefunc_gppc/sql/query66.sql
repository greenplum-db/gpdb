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

    -- Source table history contains null values
    insert into history values (null,'2011-08-24'), (4,null), (null, null);

    SELECT id FROM project( TABLE( SELECT id FROM history 
    where id is null
    order by id scatter by id, time), 1) order by 1 desc;

    delete from history where id is null or time is null;
