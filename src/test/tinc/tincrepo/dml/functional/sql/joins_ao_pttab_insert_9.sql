-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test9: Join on the non-distribution key of target table
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_ao_pt_r;
SELECT COUNT(*) FROM (SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.a)foo;
INSERT INTO dml_ao_pt_r SELECT dml_ao_pt_r.* FROM dml_ao_pt_r,dml_ao_pt_s  WHERE dml_ao_pt_r.b = dml_ao_pt_s.a;
SELECT COUNT(*) FROM dml_ao_pt_r;
