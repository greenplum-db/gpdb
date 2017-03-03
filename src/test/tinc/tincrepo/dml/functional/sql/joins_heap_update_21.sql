-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test21: Update with joins on multiple table
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
UPDATE dml_heap_r SET a = dml_heap_r.b+1 FROM dml_heap_p,dml_heap_s WHERE dml_heap_r.b = dml_heap_s.b and dml_heap_r.a = dml_heap_p.b+1;
