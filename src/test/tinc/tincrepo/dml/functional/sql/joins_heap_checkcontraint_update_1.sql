-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test1: Update data that satisfy the check constraints 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(d) FROM dml_heap_check_s;
UPDATE dml_heap_check_s SET d = dml_heap_check_s.d * 1 FROM dml_heap_check_r WHERE dml_heap_check_r.a = dml_heap_check_s.b;
SELECT SUM(d) FROM dml_heap_check_s;
