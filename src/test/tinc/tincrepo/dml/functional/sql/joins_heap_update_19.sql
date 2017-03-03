-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description update_test19: Negative test - Update with aggregates 
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(b) FROM dml_heap_r;
UPDATE dml_heap_r SET b = MAX(dml_heap_s.b) FROM dml_heap_s WHERE dml_heap_r.b = dml_heap_s.a;
SELECT SUM(b) FROM dml_heap_r;
