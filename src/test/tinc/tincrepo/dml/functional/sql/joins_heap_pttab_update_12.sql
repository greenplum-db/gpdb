-- @author prabhd  
-- @created 2012-12-05 12:00:00  
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @description update_test12: Update and limit in subquery
-- @execute_all_plans True
-- @db_name dmldb
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
SELECT DISTINCT(b) FROM dml_heap_pt_s ORDER BY 1 LIMIT 1;
UPDATE dml_heap_pt_r SET a = (SELECT DISTINCT(b) FROM dml_heap_pt_s ORDER BY 1 LIMIT 1) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
