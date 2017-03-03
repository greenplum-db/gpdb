-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test3: Negative Test - Multiple constant tuple Inserts
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(NULL,NULL,NULL),(10,10,'text');
SELECT COUNT(*) FROM dml_co_pt_r;
