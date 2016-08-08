-- Currently ETF can take one only one anytable type input.
    CREATE OR REPLACE FUNCTION transform3(a anytable, b anytable)
      RETURNS TABLE (b text, a int)
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;
