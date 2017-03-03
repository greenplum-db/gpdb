-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test21: Negative test case. INSERT has more expressions than target columns
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_pt_s;
SELECT COUNT(*) FROM (SELECT COUNT(*) as a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a GROUP BY dml_co_pt_r.a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d)foo;
INSERT INTO dml_co_pt_s SELECT COUNT(*) as a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d FROM dml_co_pt_r, dml_co_pt_s WHERE dml_co_pt_s.a = dml_co_pt_r.a GROUP BY dml_co_pt_r.a, dml_co_pt_r.b, dml_co_pt_r.c, dml_co_pt_r.d;
SELECT COUNT(*) FROM dml_co_pt_s;
