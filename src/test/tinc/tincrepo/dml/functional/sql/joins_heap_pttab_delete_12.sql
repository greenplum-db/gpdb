-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test12: Delete with join in using
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_r;
DELETE FROM dml_heap_pt_r USING (SELECT 1 as t) foo WHERE foo.t = dml_heap_pt_r.a; 
SELECT COUNT(*) FROM dml_heap_pt_r;
