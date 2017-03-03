-- @author prabhd
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test13: Delete with multiple joins
-- @execute_all_plans True
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_r;
DELETE FROM dml_heap_r USING dml_heap_s,dml_heap_p WHERE dml_heap_r.a = dml_heap_s.b and dml_heap_r.b = dml_heap_p.a;
SELECT COUNT(*) FROM dml_heap_r;
