-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test5: Update and reset the value
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_pt_r)foo;
UPDATE dml_heap_pt_r SET a = a;
SELECT COUNT(*) FROM (SELECT DISTINCT(a) FROM dml_heap_pt_r)foo;
