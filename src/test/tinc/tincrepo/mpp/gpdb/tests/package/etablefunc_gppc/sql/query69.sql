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

    drop view if exists history_v;
    create view history_v as (
    select * from history order by id);

    -- History table self-join
    SELECT * FROM project( TABLE(SELECT * FROM history h1, history h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3)
    WHERE id <3 ORDER BY 1;

    -- Join history table with history_v view
    SELECT * FROM project( TABLE(SELECT * FROM history h1, history_v h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3)
    WHERE id <3 ORDER BY 1;

  -- Join history table with ETF, using join format
    SELECT * from history h1 join project( TABLE(SELECT * FROM history h1, history_v h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3) h3
    ON h1.id = h3.id
    order by h1.time limit 5;

    -- Join history table with ETF, put join condition in where clause
    SELECT * from history h1, project( TABLE(SELECT * FROM history h1, history_v h2
        WHERE h1.id = h2.id and h1.time=h2.time 
        ORDER BY h1.id SCATTER BY h2.time), 3) h3
    WHERE h1.id = h3.id
    order by h1.time desc limit 5;

