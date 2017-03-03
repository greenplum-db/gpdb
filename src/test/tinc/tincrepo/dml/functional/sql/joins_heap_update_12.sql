-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test12: Update and limit in subquery
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = (SELECT DISTINCT(b) FROM dml_heap_s ORDER BY 1 LIMIT 1) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_r WHERE a = 1;
