-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test2: Negative test - Insert default value violates check constraint
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_check_p;
INSERT INTO dml_co_check_p select generate_series(1,100),'p', generate_series(1,100);
SELECT COUNT(*) FROM dml_co_check_p;
