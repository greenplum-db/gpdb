--  ETF using SPI with describe function and user context

CREATE OR REPLACE FUNCTION describe_spi(internal) RETURNS internal AS '$libdir/tabfunc_gppc_demo' LANGUAGE c;
CREATE OR REPLACE FUNCTION project_spi(anytable, text) RETURNS SETOF record AS '$libdir/tabfunc_gppc_demo' LANGUAGE c WITH(describe=describe_spi);
SELECT * FROM project_spi(TABLE(SELECT a::text FROM generate_series(1, 10)a SCATTER BY a), 'SELECT $$foo$$') ORDER BY 1;

