-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test3: Negative test - Insert default value violates NOT NULL constraint
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_check_s;
INSERT INTO dml_co_check_s values(default,1,'nn',1.0000);
SELECT COUNT(*) FROM dml_co_check_s;
