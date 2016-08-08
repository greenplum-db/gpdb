-- Calling the error callback function (tfcallback()) from table function (project_errorcallback())

CREATE OR REPLACE FUNCTION project_errorcallback(anytable, OUT int, OUT int) RETURNS SETOF record AS '$libdir/tabfunc_gppc_demo' LANGUAGE c;

SELECT * FROM project_errorcallback(TABLE(SELECT CASE WHEN a < 10 THEN a END, a FROM generate_series(1, 10)a SCATTER BY a));

SELECT * FROM project_errorcallback(TABLE(SELECT a, a FROM generate_series(1, 5)a SCATTER BY a)) ORDER BY 1;
