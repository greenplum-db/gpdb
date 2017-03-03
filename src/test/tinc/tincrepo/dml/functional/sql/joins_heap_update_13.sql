-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test13: Update multiple columns 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_r WHERE b is NULL;
SELECT dml_heap_s.a + 10 FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a ORDER BY 1 LIMIT 1;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
UPDATE dml_heap_r SET a = dml_heap_s.a + 10 ,b = NULL FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.a;
SELECT SUM(a) FROM dml_heap_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_r WHERE b is NULL;
