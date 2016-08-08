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

    -- Negative: $1 is not a constant
    PREPARE p4 AS SELECT * FROM project( TABLE( SELECT * FROM history ), $1);
    -- ERROR:  unable to resolve type for function (tabfunc_gppc_demo.c:174)
    -- LINE 1: PREPARE p4 AS SELECT * FROM project( TABLE( SELECT * FROM hi...

    -- Negative: $1 is not a constant
    PREPARE p5(integer) AS SELECT * FROM project( TABLE( SELECT * FROM history ), $1);
    -- ERROR:  unable to resolve type for function (tabfunc_gppc_demo.c:174)
    -- LINE 1: PREPARE p5(integer) AS SELECT * FROM project( TABLE( SELECT ...

    -- Positive: can prepare with a dynamic result set MPP-16643
    PREPARE p6 AS SELECT * FROM project( TABLE( SELECT * FROM history ), 1) order by id;
    EXECUTE p6;
    EXECUTE p6;
    EXECUTE p6;
    EXECUTE p6;
    DEALLOCATE p6;
