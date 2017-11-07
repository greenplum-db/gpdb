--
-- Test the boundaries of some built-in datatypes
--

set datestyle to 'ISO, DMY';
set intervalstyle to 'postgres';

--
-- bit varying
--
DROP TABLE IF EXISTS dml_bitvarying;
CREATE TABLE dml_bitvarying(a bit varying(2)) distributed by (a);

-- Simple DML
INSERT INTO dml_bitvarying VALUES('11');
SELECT * FROM dml_bitvarying ORDER BY 1;
UPDATE dml_bitvarying SET a = '00';
SELECT * FROM dml_bitvarying ORDER BY 1;

-- out of range values
INSERT INTO dml_bitvarying VALUES('111');
SELECT * FROM dml_bitvarying ORDER BY 1;
UPDATE dml_bitvarying SET a = '000';
SELECT * FROM dml_bitvarying ORDER BY 1;

--
-- Interval
--
DROP TABLE IF EXISTS dml_interval;
CREATE TABLE dml_interval(a interval) distributed by (a);
-- SIMPLE INSERTS
INSERT INTO dml_interval VALUES('178000000 years');
SELECT * FROM dml_interval ORDER BY 1;
INSERT INTO dml_interval VALUES('-178000000 years');
SELECT * FROM dml_interval ORDER BY 1;
UPDATE dml_interval SET a = '-178000000 years';
SELECT * FROM dml_interval ORDER BY 1;

-- out of range values
INSERT INTO dml_interval VALUES('178000000 years 1 month');
SELECT * FROM dml_interval ORDER BY 1;
INSERT INTO dml_interval VALUES('-178000000 years 1 month');
SELECT * FROM dml_interval ORDER BY 1;
UPDATE dml_interval SET a = '-178000000 years 1 month';
SELECT * FROM dml_interval ORDER BY 1;

--
-- Numeric
--
DROP TABLE IF EXISTS dml_numeric;
CREATE TABLE dml_numeric(a numeric) distributed by (a);

-- Simple DML
INSERT INTO dml_numeric VALUES (10e+1000);
SELECT * FROM dml_numeric ORDER BY 1;
INSERT INTO dml_numeric VALUES (1e-1000);
SELECT * FROM dml_numeric ORDER BY 1;

-- out of range values
INSERT INTO dml_numeric VALUES (1e+10000);
SELECT * FROM dml_numeric ORDER BY 1;
UPDATE dml_numeric SET a = 1e+10000;
SELECT * FROM dml_numeric ORDER BY 1;

--
-- Numeric, with a typmod
--
DROP TABLE IF EXISTS dml_numeric2;
CREATE TABLE dml_numeric2(a numeric(5,2)) distributed by (a);

-- Simple DML
INSERT  INTO  dml_numeric2 VALUES (1.00e+2);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+1;
SELECT * FROM dml_numeric2 ORDER BY 1;

-- out of range values
INSERT  INTO  dml_numeric2 VALUES (1.00e+3);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+3;
SELECT * FROM dml_numeric2 ORDER BY 1;

--
-- Timestamp without timezone
--
DROP TABLE IF EXISTS dml_timestamp;
CREATE TABLE dml_timestamp( a timestamp) distributed by (a);

-- Simple DML
INSERT INTO dml_timestamp VALUES (to_date('2012-02-31', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamp ORDER BY 1;
INSERT INTO dml_timestamp VALUES (to_date('4714-01-27 AD', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamp ORDER BY 1;
UPDATE dml_timestamp SET a = to_date('2012-02-31', 'YYYY-MM-DD BC');
SELECT * FROM dml_timestamp ORDER BY 1;

-- out of range values
INSERT INTO dml_timestamp VALUES ('294277-01-27 AD'::timestamp);
SELECT * FROM dml_timestamp ORDER BY 1;
UPDATE dml_timestamp SET a = '294277-01-27 AD'::timestamp;
SELECT * FROM dml_timestamp ORDER BY 1;

--
-- Timestamp with timezone
--
DROP TABLE IF EXISTS dml_timestamptz;
CREATE TABLE dml_timestamptz( a timestamptz) distributed by (a);

-- Simple DML
INSERT INTO dml_timestamptz VALUES (to_date('4714-01-27 AD', 'YYYY-MM-DD BC'));
SELECT * FROM dml_timestamptz ORDER BY 1;
UPDATE dml_timestamptz SET a = to_date('4714-01-27 AD', 'YYYY-MM-DD BC');
SELECT * FROM dml_timestamptz ORDER BY 1;

-- out of range values
INSERT INTO dml_timestamptz VALUES ('294277-01-27 AD'::timestamptz);
SELECT * FROM dml_timestamptz ORDER BY 1;
INSERT INTO dml_timestamptz VALUES ('4714-01-27 BC'::timestamptz);
SELECT * FROM dml_timestamptz ORDER BY 1;
UPDATE dml_timestamptz SET a = '4714-01-27 BC'::timestamptz;
SELECT * FROM dml_timestamptz ORDER BY 1;
