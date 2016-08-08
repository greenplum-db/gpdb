-- Negative: create ETF with distribution and/or ordering defined at function create time.
-- The followings should not be allowed.

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      scatter randomly
      RETURNS SETOF outtable
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      RETURNS SETOF outtable
      scatter randomly
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      order by a
      RETURNS SETOF outtable
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      RETURNS SETOF outtable
      order by a
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      partition by a
      RETURNS SETOF outtable
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION transform_tmp(a anytable)
      RETURNS SETOF outtable
      partition by a
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

