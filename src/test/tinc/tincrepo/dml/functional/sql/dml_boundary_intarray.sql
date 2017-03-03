-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.473
-- @description test1: Boundary test for integer array
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_intarr;
CREATE TABLE dml_intarr (a integer[] DEFAULT '{5,4,3,2,1}') distributed by (a);

-- Simple DML
INSERT INTO dml_intarr VALUES('{6,7,8,9,10}');
SELECT * FROM dml_intarr ORDER BY 1;
INSERT INTO dml_intarr DEFAULT VALUES;
SELECT * FROM dml_intarr ORDER BY 1;
SELECT a[1] FROM dml_intarr ORDER BY 1;
UPDATE dml_intarr SET a = '{11,12}';
SELECT * FROM dml_intarr ORDER BY 1;
UPDATE dml_intarr SET a[1] = 111;
SELECT a[1] FROM dml_intarr ORDER BY 1;
