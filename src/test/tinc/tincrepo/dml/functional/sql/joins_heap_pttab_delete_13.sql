-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Delete with multiple joins
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING dml_heap_pt_s,dml_heap_pt_p WHERE dml_heap_pt_r.a = dml_heap_pt_s.b and dml_heap_pt_r.b = dml_heap_pt_p.a;
SELECT COUNT(*) FROM dml_heap_pt_r;
