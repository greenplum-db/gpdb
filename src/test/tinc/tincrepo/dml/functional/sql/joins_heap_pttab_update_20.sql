-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test20: Negative test - Update WHERE join returns more than one tuple with different values.
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(a) FROM dml_heap_pt_v;
UPDATE dml_heap_pt_v SET a = dml_heap_pt_u.a FROM dml_heap_pt_u WHERE dml_heap_pt_u.b = dml_heap_pt_v.b;
SELECT SUM(a) FROM dml_heap_pt_v;
