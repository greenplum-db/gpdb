-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test19: Insert 0 rows
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_r;
INSERT INTO dml_co_r SELECT dml_co_r.* FROM dml_co_r,dml_co_s WHERE dml_co_r.b = dml_co_s.a and false;
SELECT COUNT(*) FROM dml_co_r;
