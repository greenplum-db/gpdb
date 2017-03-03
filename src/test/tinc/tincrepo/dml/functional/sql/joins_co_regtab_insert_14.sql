-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test14: Join with Aggregate
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM (SELECT COUNT(*) + dml_co_s.a, dml_co_r.b + dml_co_r.a ,'text' FROM dml_co_r, dml_co_s WHERE dml_co_r.b = dml_co_s.b GROUP BY dml_co_s.a,dml_co_r.b,dml_co_r.a)foo;
INSERT INTO dml_co_r SELECT COUNT(*) + dml_co_s.a, dml_co_r.b + dml_co_r.a ,'text' FROM dml_co_r, dml_co_s WHERE dml_co_r.b = dml_co_s.b GROUP BY dml_co_s.a,dml_co_r.b,dml_co_r.a ;
SELECT COUNT(*) FROM dml_co_r;
