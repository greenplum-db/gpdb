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

-- Invalid projection position 

    SELECT * FROM project( TABLE( SELECT * FROM history ), NULL);
    SELECT * FROM project( TABLE( SELECT * FROM history ), -1);
    SELECT * FROM project( TABLE( SELECT * FROM history ), 0);
    SELECT * FROM project( TABLE( SELECT * FROM history ), 25);
    SELECT * FROM project( TABLE( SELECT time FROM history ), 2);
    SELECT * FROM project( TABLE( SELECT * FROM t1 ), (ARRAY[2,3])[1]);

-- The following queries should work
    SELECT * FROM project( TABLE( SELECT * FROM history ), 1+1) order by time;

    SELECT * FROM project( TABLE( SELECT * FROM t1 ),
      CASE 1 WHEN 2 THEN 1 ELSE GREATEST(1, COALESCE(1+1)) END) 
      order by b limit 10;

    SELECT * FROM project( TABLE( SELECT * FROM t1 ),
      CASE WHEN 3 IS NOT NULL AND 1 IN (1, 2) THEN floor(NULLIF(2, 3))::int END)
      order by b limit 10;

