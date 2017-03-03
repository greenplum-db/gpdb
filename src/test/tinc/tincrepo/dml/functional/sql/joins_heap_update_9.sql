-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test9: Update with Joins and set to constant value
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_s WHERE b = 10;
UPDATE dml_heap_s SET b = 10 FROM dml_heap_r WHERE dml_heap_r.b = dml_heap_s.a;
SELECT COUNT(*) FROM dml_heap_s WHERE b = 10;
