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

    -- ETF subquery using view
    drop view if exists history_v;
    create view history_v as (
    select * from history order by id);

    SELECT * FROM project( 
        TABLE( SELECT * FROM history_v order by id scatter by id), 1) 
    where id < 3 order by 1;

    DROP FUNCTION project(anytable, integer);
    DROP FUNCTION project_desc(internal);
