-- Create ETF using return syntax: TABLE (argname argtype).
--    CREATE FUNCTION tabfunc2(t anytable) 
--    RETURNS TABLE (x int, y text) ...
    CREATE OR REPLACE FUNCTION transform3(a anytable)
      RETURNS TABLE (b text, a int)
      AS '$libdir/tabfunc_gppc_demo', 'mytransform'
      LANGUAGE C;

    select * from transform3(TABLE(select * from intable where id<3));
