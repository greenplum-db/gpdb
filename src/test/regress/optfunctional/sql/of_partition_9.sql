
-- start_ignore
create schema sql_partition_401_450;
set search_path to sql_partition_401_450;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_float;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_float VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_float DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_float VALUES(2.00,2.00,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_float ADD COLUMN col5 float DEFAULT 2.00;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_float SELECT 2.00,2.00,'c',2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_float SET col5 = 1.00 WHERE col2 = 2.00 AND col1 = 2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_float ADD PARTITION partfour start(30.00) end(40.00);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_float ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_float SELECT 35.00,35.00,'d',35.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_float SET col5 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_float SET col2 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_int;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int VALUES(20000,20000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int VALUES(20000,20000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int ADD COLUMN col5 int DEFAULT 20000;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int SELECT 20000,20000,'c',20000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int SET col5 = 10000 WHERE col2 = 20000 AND col1 = 20000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int WHERE col5 = 10000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int ADD PARTITION partfour start(30001) end(40001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int SELECT 35000,35000,'d',35000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int SET col5 = 10000 WHERE col2 = 35000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int SET col2 = 10000 WHERE col2 = 35000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int WHERE col5 = 10000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_int4;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int4 VALUES(20000000,20000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int4 VALUES(20000000,20000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ADD COLUMN col5 int4 DEFAULT 20000000;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int4 SELECT 20000000,20000000,'c',20000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 SET col5 = 10000000 WHERE col2 = 20000000 AND col1 = 20000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 WHERE col5 = 10000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ADD PARTITION partfour start(300000001) end(400000001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int4 SELECT 35000000,35000000,'d',35000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 SET col5 = 10000000 WHERE col2 = 35000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int4 SET col2 = 10000000 WHERE col2 = 35000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 WHERE col5 = 10000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_int8;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 VALUES(2000000000000000000,2000000000000000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ADD COLUMN col5 int8 DEFAULT 2000000000000000000;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SELECT 2000000000000000000,2000000000000000000,'c',2000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SET col5 = 1000000000000000000 WHERE col2 = 2000000000000000000 AND col1 = 2000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 WHERE col5 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SELECT 3500000000000000000,3500000000000000000,'d',3500000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SET col5 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 WHERE col5 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_interval;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval VALUES('1 hour','1 hour','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval VALUES('1 hour','1 hour','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval ADD COLUMN col5 interval DEFAULT '1 hour';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval SELECT '1 hour','1 hour','c','1 hour';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_interval SET col5 = '12 hours' WHERE col2 = '1 hour' AND col1 = '1 hour';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval WHERE col5 = '12 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_interval ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_interval SELECT '14 hours','14 hours','d','14 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_interval SET col5 = '12 hours' WHERE col2 = '14 hours' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_interval SET col2 = '12 hours' WHERE col2 = '14 hours' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval WHERE col5 = '12 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_numeric;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric VALUES(2.000000,2.000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ADD COLUMN col5 numeric DEFAULT 2.000000;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SELECT 2.000000,2.000000,'c',2.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SET col5 = 1.000000 WHERE col2 = 2.000000 AND col1 = 2.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric WHERE col5 = 1.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SELECT 35.000000,35.000000,'d',35.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SET col5 = 1.000000 WHERE col2 = 35.000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric WHERE col5 = 1.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_text;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_text DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_text VALUES('abcdefghijklmnop','abcdefghijklmnop','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_text ADD COLUMN col5 text DEFAULT 'abcdefghijklmnop';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_text SELECT 'abcdefghijklmnop','abcdefghijklmnop','c','abcdefghijklmnop';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_text SET col5 = 'qrstuvwxyz' WHERE col2 = 'abcdefghijklmnop' AND col1 = 'abcdefghijklmnop';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text WHERE col5 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_text ADD PARTITION partfour VALUES('xyz');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_text ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_text SELECT 'xyz','xyz','d','xyz';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_text SET col5 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text WHERE col5 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_time;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_time DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_time VALUES('12:00:00','12:00:00','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_time ADD COLUMN col5 time DEFAULT '12:00:00';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_time SELECT '12:00:00','12:00:00','c','12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_time SET col5 = '1:00:00' WHERE col2 = '12:00:00' AND col1 = '12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time WHERE col5 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_time ADD PARTITION partfour start('11:00:00') end('12:00:00');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_time ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_time SELECT '11:30:00','11:30:00','d','11:30:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_time SET col5 = '1:00:00' WHERE col2 = '11:30:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time WHERE col5 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ADD COLUMN col5 timestamp DEFAULT '2013-12-31 12:00:00';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp SELECT '2013-12-31 12:00:00','2013-12-31 12:00:00','c','2013-12-31 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp SET col5 = '2014-01-01 12:00:00' WHERE col2 = '2013-12-31 12:00:00' AND col1 = '2013-12-31 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp WHERE col5 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','d','2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp SET col5 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp WHERE col5 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ADD COLUMN col5 timestamptz DEFAULT '2013-12-31 12:00:00 PST';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz SELECT '2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','c','2013-12-31 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz SET col5 = '2014-01-01 12:00:00 PST' WHERE col2 = '2013-12-31 12:00:00 PST' AND col1 = '2013-12-31 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz WHERE col5 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','d','2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz SET col5 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz WHERE col5 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char VALUES('g','g','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char VALUES('g','g','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ADD COLUMN col5 char DEFAULT 'g';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_char;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_char on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SELECT 'g','g','c','g';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SET col5 = 'a' WHERE col2 = 'g' AND col1 = 'g';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char WHERE col5 = 'a';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ADD PARTITION partfour VALUES('y','z','-');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SELECT 'z','z','d','z';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SET col5 = 'a' WHERE col2 = 'z' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char SET col2 = 'a' WHERE col2 = 'z' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char WHERE col5 = 'a';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date VALUES('2013-12-30','2013-12-30','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date VALUES('2013-12-30','2013-12-30','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ADD COLUMN col5 date DEFAULT '2013-12-30';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_date;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_date on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date SELECT '2013-12-30','2013-12-30','c','2013-12-30';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date SET col5 = '2014-01-01' WHERE col2 = '2013-12-30' AND col1 = '2013-12-30';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date WHERE col5 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date SELECT '2014-02-10','2014-02-10','d','2014-02-10';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date SET col5 = '2014-01-01' WHERE col2 = '2014-02-10' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date WHERE col5 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal VALUES(2.00,2.00,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ADD COLUMN col5 decimal DEFAULT 2.00;

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_decimal;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_decimal on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal SELECT 2.00,2.00,'c',2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal SET col5 = 1.00 WHERE col2 = 2.00 AND col1 = 2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal SELECT 35.00,35.00,'d',35.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal SET col5 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float VALUES(2.00,2.00,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ADD COLUMN col5 float DEFAULT 2.00;

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_float;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_float on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float SELECT 2.00,2.00,'c',2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float SET col5 = 1.00 WHERE col2 = 2.00 AND col1 = 2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ADD PARTITION partfour start(30.00) end(40.00);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float SELECT 35.00,35.00,'d',35.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float SET col5 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float SET col2 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int VALUES(10000,10000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int VALUES(10000,10000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ADD COLUMN col5 int DEFAULT 10000;

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_int;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_int on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int SELECT 10000,10000,'c',10000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int SET col5 = 20000 WHERE col2 = 10000 AND col1 = 10000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int WHERE col5 = 20000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ADD PARTITION partfour start(30001) end(40001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int SELECT 35000,35000,'d',35000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int SET col5 = 20000 WHERE col2 = 35000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int SET col2 = 20000 WHERE col2 = 35000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int WHERE col5 = 20000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 VALUES(20000000,20000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 VALUES(20000000,20000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ADD COLUMN col5 int4 DEFAULT 20000000;

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_int4;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_int4 on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 SELECT 20000000,20000000,'c',20000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 SET col5 = 10000000 WHERE col2 = 20000000 AND col1 = 20000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 WHERE col5 = 10000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ADD PARTITION partfour start(300000001) end(400000001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 SELECT 350000000,350000000,'d',350000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 SET col5 = 10000000 WHERE col2 = 350000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 SET col2 = 10000000 WHERE col2 = 350000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 WHERE col5 = 10000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 VALUES(200000000000000000,200000000000000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 VALUES(200000000000000000,200000000000000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ADD COLUMN col5 int8 DEFAULT 200000000000000000;

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_int8;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_int8 on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 SELECT 200000000000000000,200000000000000000,'c',200000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 SET col5 = 1000000000000000000 WHERE col2 = 200000000000000000 AND col1 = 200000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 WHERE col5 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 SELECT 3500000000000000000,3500000000000000000,'d',3500000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 SET col5 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 WHERE col5 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval VALUES('10 secs','10 secs','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval VALUES('10 secs','10 secs','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ADD COLUMN col5 interval DEFAULT '10 secs';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_interval;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_interval on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval SELECT '10 secs','10 secs','c','10 secs';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval SET col5 = '11 hours' WHERE col2 = '10 secs' AND col1 = '10 secs';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval WHERE col5 = '11 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval SELECT '14 hours','14 hours','d','14 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval SET col5 = '11 hours' WHERE col2 = '14 hours' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval SET col2 = '11 hours' WHERE col2 = '14 hours' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval WHERE col5 = '11 hours';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric VALUES(2.000000,2.000000,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ADD COLUMN col5 numeric DEFAULT 2.000000;

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_numeric;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_numeric on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric SELECT 2.000000,2.000000,'c',2.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric SET col5 = 1.000000 WHERE col2 = 2.000000 AND col1 = 2.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric WHERE col5 = 1.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric SELECT 35.000000,35.000000,'d',35.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric SET col5 = 1.000000 WHERE col2 = 35.000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric WHERE col5 = 1.000000;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time VALUES('12:00:00','12:00:00','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ADD COLUMN col5 time DEFAULT '12:00:00';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_time;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_time on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SELECT '12:00:00','12:00:00','c','12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SET col5 = '1:00:00' WHERE col2 = '12:00:00' AND col1 = '12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time WHERE col5 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ADD PARTITION partfour start('11:00:00') end('12:00:00');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SELECT '11:30:00','11:30:00','d','11:30:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SET col5 = '1:00:00' WHERE col2 = '11:30:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time WHERE col5 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ADD COLUMN col5 timestamp DEFAULT '2013-12-30 12:00:00';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_timestamp;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_timestamp on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp SELECT '2013-12-30 12:00:00','2013-12-30 12:00:00','c','2013-12-30 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp SET col5 = '2014-01-01 12:00:00' WHERE col2 = '2013-12-30 12:00:00' AND col1 = '2013-12-30 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp WHERE col5 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','d','2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp SET col5 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp WHERE col5 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ADD COLUMN col5 timestamptz DEFAULT '2013-12-30 12:00:00 PST';

DROP INDEX IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_timestamptz;
CREATE INDEX mpp21090_pttab_dropcol_addcol_addpt_idx_dml_idx_timestamptz on mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz(col5);

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz SELECT '2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','c','2013-12-30 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz SET col5 = '2014-01-01 12:00:00 PST' WHERE col2 = '2013-12-30 12:00:00 PST' AND col1 = '2013-12-30 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz WHERE col5 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','d','2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz SET col5 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz WHERE col5 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_idx_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_char;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_char VALUES('x','x','a','x',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_char DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_char SELECT 'z','b','z', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_char SET col4 = '-' WHERE col2 = 'z' AND col4 = 'z';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_char SET col2 = '-' WHERE col2 = 'z' AND col4 = '-';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_date;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_date VALUES('2013-12-31','2013-12-31','a','2013-12-31',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_date DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_date SELECT '2014-02-10','b','2014-02-10', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_date SET col4 = '2014-01-01' WHERE col2 = '2014-02-10' AND col4 = '2014-02-10';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col4 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_decimal;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_decimal VALUES(2.00,2.00,'a',2.00,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_decimal SELECT 35.00,'b',35.00, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_decimal SET col4 = 1.00 WHERE col2 = 35.00 AND col4 = 35.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col4 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_float;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_float VALUES(2.00,2.00,'a',2.00,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_float DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_float SELECT 35.00,'b',35.00, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_float SET col4 = 1.00 WHERE col2 = 35.00 AND col4 = 35.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_float SET col2 = 1.00 WHERE col2 = 35.00 AND col4 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_char;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_char VALUES('g','g','a','g',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_char;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_char on mpp21090_pttab_dropfirstcol_addpt_index_char(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_char DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_char SELECT 'z','b','z', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_char SET col4 = 'a' WHERE col2 = 'z' AND col4 = 'z';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_char SET col2 = 'a' WHERE col2 = 'z' AND col4 = 'a';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_char WHERE col2 = 'a';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_date;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_date VALUES('2013-12-30','2013-12-30','a','2013-12-30',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_date;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_date on mpp21090_pttab_dropfirstcol_addpt_index_date(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_date DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_date SELECT '2014-02-10','b','2014-02-10', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_date SET col4 = '2014-01-01' WHERE col2 = '2014-02-10' AND col4 = '2014-02-10';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col4 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_decimal;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_decimal VALUES(2.00,2.00,'a',2.00,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_decimal;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_decimal on mpp21090_pttab_dropfirstcol_addpt_index_decimal(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_decimal SELECT 35.00,'b',35.00, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_decimal SET col4 = 1.00 WHERE col2 = 35.00 AND col4 = 35.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col4 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_float;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_float VALUES(2.00,2.00,'a',2.00,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_float;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_float on mpp21090_pttab_dropfirstcol_addpt_index_float(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_float DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_float SELECT 35.00,'b',35.00, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_float SET col4 = 1.00 WHERE col2 = 35.00 AND col4 = 35.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_float SET col2 = 1.00 WHERE col2 = 35.00 AND col4 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_int;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int VALUES(10000,10000,'a',10000,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_int;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_int on mpp21090_pttab_dropfirstcol_addpt_index_int(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int SELECT 35000,'b',35000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int SET col4 = 20000 WHERE col2 = 35000 AND col4 = 35000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int SET col2 = 20000 WHERE col2 = 35000 AND col4 = 20000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_int WHERE col2 = 20000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_int4;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int4 VALUES(20000000,20000000,'a',20000000,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_int4;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_int4 on mpp21090_pttab_dropfirstcol_addpt_index_int4(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int4 DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int4 SELECT 350000000,'b',350000000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int4 SET col4 = 10000000 WHERE col2 = 350000000 AND col4 = 350000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int4 SET col2 = 10000000 WHERE col2 = 350000000 AND col4 = 10000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_int8;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_int8;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_int8 on mpp21090_pttab_dropfirstcol_addpt_index_int8(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_int8 SELECT 3500000000000000000,'b',3500000000000000000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int8 SET col4 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col4 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col4 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_interval;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_interval VALUES('10 secs','10 secs','a','10 secs',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_interval;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_interval on mpp21090_pttab_dropfirstcol_addpt_index_interval(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_interval DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_interval SELECT '14 hours','b','14 hours', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_interval SET col4 = '11 hours' WHERE col2 = '14 hours' AND col4 = '14 hours';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_interval SET col2 = '11 hours' WHERE col2 = '14 hours' AND col4 = '11 hours';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_interval WHERE col2 = '11 hours';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_numeric;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_numeric VALUES(2.000000,2.000000,'a',2.000000,0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_numeric;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_numeric on mpp21090_pttab_dropfirstcol_addpt_index_numeric(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_numeric DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_numeric SELECT 35.000000,'b',35.000000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_numeric SET col4 = 1.000000 WHERE col2 = 35.000000 AND col4 = 35.000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col4 = 1.000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_time;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_time VALUES('12:00:00','12:00:00','a','12:00:00',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_time;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_time on mpp21090_pttab_dropfirstcol_addpt_index_time(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_time DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_time SELECT '11:30:00','b','11:30:00', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_time SET col4 = '1:00:00' WHERE col2 = '11:30:00' AND col4 = '11:30:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col4 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_timestamp;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a','2013-12-30 12:00:00',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_timestamp;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_timestamp on mpp21090_pttab_dropfirstcol_addpt_index_timestamp(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_timestamp DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_timestamp SELECT '2014-02-10 12:00:00','b','2014-02-10 12:00:00', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_timestamp SET col4 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col4 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col4 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_timestamptz;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_index_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a','2013-12-30 12:00:00 PST',0);

DROP INDEX IF EXISTS mpp21090_pttab_dropfirstcol_addpt_index_idx_timestamptz;
CREATE INDEX mpp21090_pttab_dropfirstcol_addpt_index_idx_timestamptz on mpp21090_pttab_dropfirstcol_addpt_index_timestamptz(col2);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_index_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_index_timestamptz SELECT '2014-02-10 12:00:00 PST','b','2014-02-10 12:00:00 PST', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_index_timestamptz SET col4 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col4 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_index_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col4 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_index_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_int;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int VALUES(20000,20000,'a',20000,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int SELECT 35000,'b',35000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_int SET col4 = 10000 WHERE col2 = 35000 AND col4 = 35000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_int SET col2 = 10000 WHERE col2 = 35000 AND col4 = 10000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_int4;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int4 VALUES(20000000,20000000,'a',20000000,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int4 DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int4 SELECT 35000000,'b',35000000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_int4 SET col4 = 10000000 WHERE col2 = 35000000 AND col4 = 35000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_int4 SET col2 = 10000000 WHERE col2 = 35000000 AND col4 = 10000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_int8;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int8 VALUES(2000000000000000000,2000000000000000000,'a',2000000000000000000,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_int8 SELECT 3500000000000000000,'b',3500000000000000000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_int8 SET col4 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col4 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col4 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_interval;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_interval VALUES('1 hour','1 hour','a','1 hour',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_interval DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_interval SELECT '14 hours','b','14 hours', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_interval SET col4 = '12 hours' WHERE col2 = '14 hours' AND col4 = '14 hours';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_interval SET col2 = '12 hours' WHERE col2 = '14 hours' AND col4 = '12 hours';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_numeric;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_numeric VALUES(2.000000,2.000000,'a',2.000000,0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_numeric DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_numeric SELECT 35.000000,'b',35.000000, 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_numeric SET col4 = 1.000000 WHERE col2 = 35.000000 AND col4 = 35.000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col4 = 1.000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_text;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 text,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a','abcdefghijklmnop',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_text DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_text SELECT 'xyz','b','xyz', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_text SET col4 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col4 = 'xyz';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col4 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_time;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_time VALUES('12:00:00','12:00:00','a','12:00:00',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_time DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_time SELECT '11:30:00','b','11:30:00', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_time SET col4 = '1:00:00' WHERE col2 = '11:30:00' AND col4 = '11:30:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col4 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_timestamp;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a','2013-12-31 12:00:00',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_timestamp DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_timestamp SELECT '2014-02-10 12:00:00','b','2014-02-10 12:00:00', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_timestamp SET col4 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col4 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col4 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropfirstcol_addpt_timestamptz;
CREATE TABLE mpp21090_pttab_dropfirstcol_addpt_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a','2013-12-31 12:00:00 PST',0);

ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_pttab_dropfirstcol_addpt_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_dropfirstcol_addpt_timestamptz SELECT '2014-02-10 12:00:00 PST','b','2014-02-10 12:00:00 PST', 1;
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropfirstcol_addpt_timestamptz SET col4 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col4 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropfirstcol_addpt_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col4 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropfirstcol_addpt_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropfirstcol_addpt_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_char;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int,
    col5 char
    
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_char VALUES('x','x','a',0,'x');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_char DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_droplastcol_addpt_char SELECT 'z','z','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_char SET col1 = '-' WHERE col2 = 'z' AND col1 = 'z';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_char SET col2 = '-' WHERE col2 = 'z' AND col1 = '-';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_date;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int,
    col5 date
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_date VALUES('2013-12-31','2013-12-31','a',0,'2013-12-31');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_date DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_date SELECT '2014-02-10','2014-02-10','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_date SET col1 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-02-10';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-01-01';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_decimal;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int,
    col5 decimal
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_droplastcol_addpt_decimal VALUES(2.00,2.00,'a',0,2.00);

ALTER TABLE mpp21090_pttab_droplastcol_addpt_decimal DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_decimal SELECT 35.00,35.00,'b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_decimal ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_decimal SET col1 = 1.00 WHERE col2 = 35.00 AND col1 = 35.00;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col1 = 1.00;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_decimal ORDER BY 1,2,3;


-- start_ignore
drop schema sql_partition_401_450 cascade;
-- end_ignore
