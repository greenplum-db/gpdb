-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test11: Update with aggregate in subquery 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = (SELECT COUNT(*) FROM dml_heap_pt_s);
SELECT COUNT(*) FROM dml_heap_pt_s;
UPDATE dml_heap_pt_s SET b = (SELECT COUNT(*) FROM dml_heap_pt_s) FROM dml_heap_pt_r WHERE dml_heap_pt_r.a = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_s WHERE b = (SELECT COUNT(*) FROM dml_heap_pt_s);
