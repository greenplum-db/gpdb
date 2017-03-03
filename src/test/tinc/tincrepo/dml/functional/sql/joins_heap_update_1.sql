-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test1: Update to constant value
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
UPDATE dml_heap_r SET c = 'text';
SELECT COUNT(*) FROM dml_heap_r WHERE c ='text';
