-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for timestamp
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_timestamp;
CREATE TABLE dml_timestamp( a timestamp) distributed by (a);

-- Simple DML
INSERT INTO dml_timestamp VALUES (to_date('2012-02-31', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamp ORDER BY 1;
INSERT INTO dml_timestamp VALUES (to_date('4714-01-27 AD', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamp ORDER BY 1;
UPDATE dml_timestamp SET a = to_date('2012-02-31', 'YYYY-MM-DD BC');
SELECT * FROM dml_timestamp ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_timestamp VALUES ('294277-01-27 AD'::timestamp);
SELECT * FROM dml_timestamp ORDER BY 1;
UPDATE dml_timestamp SET a = '294277-01-27 AD'::timestamp;
SELECT * FROM dml_timestamp ORDER BY 1;

