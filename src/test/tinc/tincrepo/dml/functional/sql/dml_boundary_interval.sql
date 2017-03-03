-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for interval
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_interval;
CREATE TABLE dml_interval( a interval) distributed by (a);
-- SIMPLE INSERTS
INSERT INTO dml_interval VALUES('178000000 years');
SELECT * FROM dml_interval ORDER BY 1;
INSERT INTO dml_interval VALUES('-178000000 years');
SELECT * FROM dml_interval ORDER BY 1;
UPDATE dml_interval SET a = '-178000000 years';
SELECT * FROM dml_interval ORDER BY 1;

--OUT OF RANGE VALUES
INSERT INTO dml_interval VALUES('178000000 years 1 month');
SELECT * FROM dml_interval ORDER BY 1;
INSERT INTO dml_interval VALUES('-178000000 years 1 month');
SELECT * FROM dml_interval ORDER BY 1;
UPDATE dml_interval SET a = '-178000000 years 1 month';
SELECT * FROM dml_interval ORDER BY 1;


