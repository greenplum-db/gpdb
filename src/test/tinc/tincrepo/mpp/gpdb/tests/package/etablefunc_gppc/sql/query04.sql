    -- create ETF with output table "hai10" does not exist at the moment
    CREATE OR REPLACE FUNCTION transform_new(a anytable)
      RETURNS SETOF hai10
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    \df transform_new

    drop function if exists transform_new(a anytable);
