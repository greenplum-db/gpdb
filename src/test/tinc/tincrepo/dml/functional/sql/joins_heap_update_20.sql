-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description update_test20: Negative test - Update WHERE join returns more than one tuple with different values.
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
DROP TABLE IF EXISTS dml_heap_u;
DROP TABLE IF EXISTS dml_heap_v;
CREATE TABLE dml_heap_u as SELECT i as a, 1 as b  FROM generate_series(1,10)i;
CREATE TABLE dml_heap_v as SELECT i as a ,i as b FROM generate_series(1,10)i;
SELECT SUM(a) FROM dml_heap_v;
UPDATE dml_heap_v SET a = dml_heap_u.a FROM dml_heap_u WHERE dml_heap_u.b = dml_heap_v.b;
SELECT SUM(a) FROM dml_heap_v;
