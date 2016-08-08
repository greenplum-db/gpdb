    -- explicit return type not suitable for dynamic type resolution
    CREATE FUNCTION x() returns int
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_describe);
    -- ERROR:  DESCRIBE only supported for functions returning "record"

    -- explicit return type (setof) not suitable for dynamic type resolution
    CREATE FUNCTION x() returns setof int
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_describe);
    -- ERROR:  DESCRIBE only supported for functions returning "record"

    -- explicit return type (TABLE) not suitable for dynamic type resolution
    CREATE FUNCTION x() returns TABLE(id integer, "time" timestamp, sessionnum integer)
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_describe);
    -- ERROR:  DESCRIBE is not supported for functions that return TABLE

    -- explicit return type (OUT PARAMS) not suitable for dynamic type resolution
    CREATE FUNCTION x(OUT id integer, OUT "time" timestamp, OUT sessionnum integer)
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project'
      LANGUAGE C 
      WITH (describe = ud_desc);
    -- ERROR:  DESCRIBE is not supported for functions with OUT parameters
