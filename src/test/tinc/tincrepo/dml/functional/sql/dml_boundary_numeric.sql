-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for numeric
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_numeric;
CREATE TABLE dml_numeric( a numeric) distributed by (a);

-- Simple DML
INSERT INTO dml_numeric VALUES (10e+1000);
SELECT * FROM dml_numeric ORDER BY 1;
INSERT INTO dml_numeric VALUES (1e-1000);
SELECT * FROM dml_numeric ORDER BY 1;

-- OUT OF RANGE
INSERT INTO dml_numeric VALUES (1e+10000);
SELECT * FROM dml_numeric ORDER BY 1;
UPDATE dml_numeric SET a = 1e+10000;
SELECT * FROM dml_numeric ORDER BY 1;



