-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test17: Delete with sub-query
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_s;
DELETE FROM dml_heap_pt_s WHERE a = (SELECT dml_heap_pt_r.a FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a);
SELECT COUNT(*) FROM dml_heap_pt_s;
