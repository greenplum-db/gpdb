-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test5: Insert with joins where the result tuples violate violates multiple check constraints
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_check_r;
SELECT COUNT(*) FROM (SELECT dml_co_check_r.a + 110 , 0, dml_co_check_r.c, NULL FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a)foo;
INSERT INTO dml_co_check_r SELECT dml_co_check_r.a + 110 , 0, dml_co_check_r.c, NULL FROM dml_co_check_r, dml_co_check_s WHERE dml_co_check_r.a = dml_co_check_s.a;
SELECT COUNT(*) FROM dml_co_check_r;
