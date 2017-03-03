-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test2: Update and set distribution key to constant
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_s)foo;
UPDATE dml_heap_s SET b = 10;
SELECT COUNT(*) FROM (SELECT DISTINCT(b) FROM dml_heap_s)foo;
