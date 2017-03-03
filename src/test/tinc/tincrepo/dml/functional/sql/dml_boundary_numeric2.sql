-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for numeric
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_numeric2;
CREATE TABLE dml_numeric2(a numeric(5,2)) distributed by (a);

-- Simple DML
INSERT  INTO  dml_numeric2 VALUES (1.00e+2);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+1;
SELECT * FROM dml_numeric2 ORDER BY 1;

--OUT OF RANGE
INSERT  INTO  dml_numeric2 VALUES (1.00e+3);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+3;
SELECT * FROM dml_numeric2 ORDER BY 1;
