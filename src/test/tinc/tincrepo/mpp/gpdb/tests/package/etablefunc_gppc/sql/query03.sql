    -- Create ETF using TABLE return syntax.
    DROP FUNCTION IF EXISTS transform(anytable);
    CREATE OR REPLACE FUNCTION transform(a anytable)
    RETURNS TABLE (a text, b int)
    AS '$libdir/tabfunc_gppc_demo', 'mytransform'
    LANGUAGE C;

SELECT * FROM transform( 
    TABLE( SELECT * FROM randtable ORDER BY id, value SCATTER BY id) 
) order by b;

