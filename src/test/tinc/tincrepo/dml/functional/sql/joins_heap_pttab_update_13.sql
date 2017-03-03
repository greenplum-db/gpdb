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
SELECT COUNT(*) FROM dml_heap_pt_r WHERE b is NULL;
SELECT dml_heap_pt_s.a + 10 FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a ORDER BY 1 LIMIT 1;
SELECT * FROM dml_heap_pt_r WHERE a = 1;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET a = dml_heap_pt_s.a + 10 ,b = NULL FROM dml_heap_pt_s WHERE dml_heap_pt_r.a + 2= dml_heap_pt_s.b;
SELECT * FROM dml_heap_pt_r WHERE a = 11 ORDER BY 1,2;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE b is NULL;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
