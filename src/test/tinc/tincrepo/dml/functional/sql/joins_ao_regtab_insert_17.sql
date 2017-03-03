-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @execute_all_plans True
-- @description test17: Insert and CASE
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_ao_r;
SELECT COUNT(*) FROM (SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_ao_p.a) as A, dml_ao_p.a as B, dml_ao_s.c as C FROM dml_ao_p, dml_ao_s WHERE dml_ao_p.a = dml_ao_s.a GROUP BY dml_ao_p.a,dml_ao_s.c)as x GROUP BY A,B,C)foo;
INSERT INTO dml_ao_r SELECT A,B, case when c ='s' then C  else NULL end as C FROM (SELECT sum(dml_ao_p.a) as A, dml_ao_p.a as B, dml_ao_s.c as C FROM dml_ao_p, dml_ao_s WHERE dml_ao_p.a = dml_ao_s.a GROUP BY dml_ao_p.a,dml_ao_s.c)as x GROUP BY A,B,C;
SELECT COUNT(*) FROM dml_ao_r;
