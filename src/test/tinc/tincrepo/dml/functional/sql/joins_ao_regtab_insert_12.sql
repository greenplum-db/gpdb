-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test12: Join on the distribution key of target table. Insert Large number of rows
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_ao_s;
SELECT COUNT(*) FROM (SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.b <> dml_ao_s.b )foo;
INSERT INTO dml_ao_s SELECT dml_ao_r.a,dml_ao_r.b,dml_ao_s.c FROM dml_ao_s INNER JOIN dml_ao_r on dml_ao_r.b <> dml_ao_s.b ;
SELECT COUNT(*) FROM dml_ao_s;
