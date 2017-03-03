-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml HAWQ 
-- @db_name dmldb
-- @description test20: Negative tests Insert column of different data type
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore
SELECT COUNT(*) FROM dml_co_r;
SELECT COUNT(*) FROM ( SELECT ('a')::int, dml_co_r.b,10 FROM dml_co_s WHERE dml_co_r.b = dml_co_s.b)foo;
INSERT INTO dml_co_r SELECT ('a')::int, dml_co_r.b,10 FROM dml_co_s WHERE dml_co_r.b = dml_co_s.b;
SELECT COUNT(*) FROM dml_co_r;
