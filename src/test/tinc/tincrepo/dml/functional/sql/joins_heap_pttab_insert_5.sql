-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test5: Insert with generate_series and NULL
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_heap_pt_r;
ALTER TABLE dml_heap_pt_r ADD DEFAULT partition def;
INSERT INTO dml_heap_pt_r values(generate_series(1,10),NULL,'text');
SELECT * FROM dml_heap_pt_r WHERE c ='text' ORDER BY 1;
ALTER TABLE dml_heap_pt_r DROP DEFAULT partition;
SELECT COUNT(*) FROM dml_heap_pt_r;
