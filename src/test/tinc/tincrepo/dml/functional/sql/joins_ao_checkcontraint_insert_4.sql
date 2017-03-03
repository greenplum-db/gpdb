-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ
-- @db_name dmldb
-- @gpopt 1.532
-- @execute_all_plans True
-- @description test4: Negative test - Insert with joins where the result tuples violate the user defined check constraint
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_ao_check_r;
SELECT COUNT(*) FROM (SELECT dml_ao_check_r.a + 110 , dml_ao_check_r.b, dml_ao_check_r.c, dml_ao_check_r.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a)foo;
INSERT INTO dml_ao_check_r SELECT dml_ao_check_r.a + 110 , dml_ao_check_r.b, dml_ao_check_r.c, dml_ao_check_r.d FROM dml_ao_check_r, dml_ao_check_s WHERE dml_ao_check_r.a = dml_ao_check_s.a;
SELECT COUNT(*) FROM dml_ao_check_r;
