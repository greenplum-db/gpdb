-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Join on the distribution key of target table. Insert Large number of rows
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_s;
SELECT COUNT(*) FROM (SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.b = dml_heap_pt_s.b)foo;
INSERT INTO dml_heap_pt_s SELECT dml_heap_pt_r.a,dml_heap_pt_r.b,dml_heap_pt_s.c FROM dml_heap_pt_s INNER JOIN dml_heap_pt_r on dml_heap_pt_r.b = dml_heap_pt_s.b ;
SELECT COUNT(*) FROM dml_heap_pt_s;
