-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.532
-- @description update_test18: Negative test - Update with sub-query returning more than one row
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT SUM(b) FROM dml_heap_pt_r;
UPDATE dml_heap_pt_r SET b = (SELECT dml_heap_pt_r.b FROM dml_heap_pt_r,dml_heap_pt_s WHERE dml_heap_pt_r.a = dml_heap_pt_s.a );
SELECT SUM(b) FROM dml_heap_pt_r;
