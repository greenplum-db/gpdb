-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description update_test17: Negative test - Update with sub-query returning more than one row
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(a) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET a = ( SELECT DISTINCT(b) FROM dml_heap_pt_s ) FROM dml_heap_pt_s WHERE dml_heap_pt_r.b = dml_heap_pt_s.a;
SELECT SUM(a) FROM dml_heap_pt_r;
