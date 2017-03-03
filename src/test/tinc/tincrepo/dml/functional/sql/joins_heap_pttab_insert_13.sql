-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test13: Join with different column order 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_r;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_s.a,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_r(b,a,c) SELECT dml_heap_pt_s.a,dml_heap_pt_r.b,'text' FROM dml_heap_pt_r,dml_heap_pt_s  WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT COUNT(*) FROM dml_heap_pt_r;
