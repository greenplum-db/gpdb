CREATE FUNCTION tablefunc_describe(internal) RETURNS internal AS '$libdir/gppc_test' LANGUAGE c;
CREATE FUNCTION tablefunc_project(anytable, int) RETURNS SETOF record AS '$libdir/gppc_test' LANGUAGE c WITH(describe=tablefunc_describe);
CREATE FUNCTION describe_spi(internal) RETURNS internal AS '$libdir/gppc_test' LANGUAGE c;
CREATE FUNCTION project_spi(anytable, text) RETURNS SETOF record AS '$libdir/gppc_test' LANGUAGE c WITH(describe=describe_spi);
CREATE FUNCTION project_errorcallback(anytable, OUT int, OUT int) RETURNS SETOF record AS '$libdir/gppc_test' LANGUAGE c;

SELECT * FROM tablefunc_project(TABLE(SELECT a, a / 10 FROM generate_series(1, 10)a SCATTER BY a), 2) ORDER BY 1;
SELECT * FROM project_spi(TABLE(SELECT a::text FROM generate_series(1, 10)a SCATTER BY a), 'SELECT $$foo$$') ORDER BY 1;
SELECT * FROM project_errorcallback(TABLE(SELECT CASE WHEN a < 10 THEN a END, a FROM generate_series(1, 10)a SCATTER BY a));
SELECT * FROM project_errorcallback(TABLE(SELECT a, a FROM generate_series(1, 5)a SCATTER BY a));
