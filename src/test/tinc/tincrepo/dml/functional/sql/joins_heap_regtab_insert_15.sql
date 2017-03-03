-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test15: Join with DISTINCT
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_s;
SELECT COUNT(*) FROM (SELECT DISTINCT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_s.a = dml_heap_r.a)foo;
INSERT INTO dml_heap_s SELECT DISTINCT dml_heap_r.a,dml_heap_r.b,dml_heap_s.c FROM dml_heap_s INNER JOIN dml_heap_r on dml_heap_s.a = dml_heap_r.a ;
SELECT COUNT(*) FROM dml_heap_s;
