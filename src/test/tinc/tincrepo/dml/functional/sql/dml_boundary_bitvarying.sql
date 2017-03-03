-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bit varying
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_bitvarying;
CREATE TABLE dml_bitvarying( a bit varying(2)) distributed by (a);

-- Simple DML
INSERT INTO dml_bitvarying VALUES('11');
SELECT * FROM dml_bitvarying ORDER BY 1;
UPDATE dml_bitvarying SET a = '00';
SELECT * FROM dml_bitvarying ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_bitvarying VALUES('111');
SELECT * FROM dml_bitvarying ORDER BY 1;
UPDATE dml_bitvarying SET a = '000';
SELECT * FROM dml_bitvarying ORDER BY 1;
