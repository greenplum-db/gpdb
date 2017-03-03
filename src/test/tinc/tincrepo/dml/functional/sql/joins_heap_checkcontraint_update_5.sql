-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test5: Negative test - Update violates multiple check constraint
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
UPDATE dml_heap_check_s SET a = 110 + dml_heap_check_s.a, b = NULL  FROM dml_heap_check_r WHERE dml_heap_check_r.b = dml_heap_check_s.b;
