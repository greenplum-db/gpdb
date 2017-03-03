-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test11: Join on distribution key of target table
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_pt_r;
SELECT COUNT(*) FROM (SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.a = dml_co_pt_s.a)foo;
INSERT INTO dml_co_pt_r SELECT dml_co_pt_r.a,dml_co_pt_r.b,dml_co_pt_s.c FROM dml_co_pt_s INNER JOIN dml_co_pt_r on dml_co_pt_r.a = dml_co_pt_s.a ;
SELECT COUNT(*) FROM dml_co_pt_r;
