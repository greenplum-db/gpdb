    -- ETF can only be created when using anytable as input type. 
    -- Negative: CREATE FUNCTION tabfunc_bad1 (x SETOF targettable) RETURNS ...
    CREATE OR REPLACE FUNCTION transform3(a SETOF intable)
      RETURNS SETOF outtable
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    -- Negative: CREATE FUNCTION tabfunc_bad2 (x TABLE(a int) ) RETURNS ...
    CREATE OR REPLACE FUNCTION transform3(x TABLE(a int, b text))
      RETURNS SETOF outtable
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;
