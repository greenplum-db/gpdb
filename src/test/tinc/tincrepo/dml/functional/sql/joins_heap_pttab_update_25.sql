-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test25: Negative test update partition key (no default partition)
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(b) FROM dml_heap_pt_r; 
UPDATE dml_heap_pt_r SET b = dml_heap_pt_r.a + 3000000 FROM dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a;
SELECT SUM(b) FROM dml_heap_pt_r; 
