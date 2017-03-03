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
SELECT COUNT(*) FROM dml_heap_r WHERE c='z';
SELECT dml_heap_s.a ,dml_heap_s.b,'z' FROM dml_heap_r,dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b ORDER BY 1,2 LIMIT 1;
UPDATE dml_heap_r SET (a,b,c) = (dml_heap_s.a ,dml_heap_s.b,'z') FROM dml_heap_s WHERE dml_heap_r.a = dml_heap_s.b;
SELECT * FROM dml_heap_r WHERE c='z' ORDER BY 1 LIMIT 1;
SELECT COUNT(*) FROM dml_heap_r WHERE c='z';
