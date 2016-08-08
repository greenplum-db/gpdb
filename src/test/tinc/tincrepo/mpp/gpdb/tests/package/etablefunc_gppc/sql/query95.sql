-- Calling error callback function errorcallback from ETF describe function (tablefunc_describe)

CREATE OR REPLACE FUNCTION tablefunc_describe(internal) RETURNS internal AS '$libdir/tabfunc_gppc_demo' LANGUAGE c;
CREATE OR REPLACE FUNCTION tablefunc_project(anytable, int) RETURNS SETOF record AS '$libdir/tabfunc_gppc_demo' LANGUAGE c WITH(describe=tablefunc_describe);
SELECT * FROM tablefunc_project(TABLE(SELECT a, a / 10 FROM generate_series(1, 10)a SCATTER BY a), 2) ORDER BY 1;
