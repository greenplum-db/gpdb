-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for varchar
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_var;
CREATE TABLE dml_var( a varchar(10485760)) distributed by (a);

-- Simple DML
INSERT INTO dml_var VALUES(repeat('x',10485760));
UPDATE dml_var SET a = 'y';
SELECT * FROM dml_var ORDER BY 1;
-- OUT OF RANGE
INSERT INTO dml_var VALUES(repeat('x',10485761));
UPDATE dml_var SET a = repeat('x',10485761);
