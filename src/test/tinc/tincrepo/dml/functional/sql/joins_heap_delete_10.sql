-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test10: Delete with join in using
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING (SELECT dml_heap_r.a FROM dml_heap_r, dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a)foo WHERE dml_heap_r.a = foo.a;
SELECT COUNT(*) FROM dml_heap_r;
