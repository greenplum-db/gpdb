-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @execute_all_plans True
-- @description update_test6: Update and generate_series
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='n';
UPDATE dml_heap_pt_r SET a = generate_series(1,10), c ='n';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE c ='n';
SELECT COUNT(*) FROM dml_heap_pt_r WHERE a = 1;
