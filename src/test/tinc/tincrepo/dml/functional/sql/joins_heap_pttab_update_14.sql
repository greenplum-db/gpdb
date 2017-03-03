-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test14: Update multiple columns 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c='z';
SELECT dml_heap_pt_s.a ,dml_heap_pt_s.b,'z' FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.b ORDER BY 1,2 LIMIT 1;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
UPDATE dml_heap_pt_r SET (a,b,c) = (dml_heap_pt_s.a ,dml_heap_pt_s.b,'z') FROM dml_heap_pt_s WHERE dml_heap_pt_r.a + 1= dml_heap_pt_s.b;
SELECT * FROM dml_heap_pt_r WHERE c='z' ORDER BY 1 LIMIT 1;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c='z';
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
