-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test24: Update the partition key and move tuples across partitions( moving tuple to default partition)
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(a) FROM dml_heap_pt_s;
UPDATE dml_heap_pt_s SET a = dml_heap_pt_r.a + 30000 FROM dml_heap_pt_r WHERE dml_heap_pt_r.b = dml_heap_pt_s.b;
SELECT SUM(a) FROM dml_heap_pt_s;
