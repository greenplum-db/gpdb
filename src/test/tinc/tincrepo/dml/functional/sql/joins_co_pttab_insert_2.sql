-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test2: Negative Test - Constant tuple Inserts( no partition )
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_pt_r;
INSERT INTO dml_co_pt_r values(NULL,NULL);
SELECT COUNT(*) FROM dml_co_pt_r;
