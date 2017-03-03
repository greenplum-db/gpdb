-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for timestamptz
\echo --start_ignore
set gp_enable_column_oriented_table=on;
\echo --end_ignore

DROP TABLE IF EXISTS dml_timestamptz;
CREATE TABLE dml_timestamptz( a timestamptz) distributed by (a);

-- Simple DML
INSERT INTO dml_timestamptz VALUES (to_date('4714-01-27 AD', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamptz ORDER BY 1;
UPDATE dml_timestamptz SET a = to_date('4714-01-27 AD', 'YYYY-MM-DD BC');
SELECT * FROM dml_timestamptz ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_timestamptz VALUES ('294277-01-27 AD'::timestamptz);
SELECT * FROM dml_timestamptz ORDER BY 1;
INSERT INTO dml_timestamptz VALUES ('4714-01-27 BC'::timestamptz);
SELECT * FROM dml_timestamptz ORDER BY 1;
UPDATE dml_timestamptz SET a = '4714-01-27 BC'::timestamptz;
SELECT * FROM dml_timestamptz ORDER BY 1;
