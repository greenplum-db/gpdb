
-- start_ignore
create schema sql_partition_451_500;
set search_path to sql_partition_451_500;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_float;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int,
    col5 float
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_droplastcol_addpt_float VALUES(2.00,2.00,'a',0,2.00);

ALTER TABLE mpp21090_pttab_droplastcol_addpt_float DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_droplastcol_addpt_float SELECT 35.00,35.00,'b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_float ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_float SET col1 = 1.00 WHERE col2 = 35.00 AND col1 = 35.00;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_float SET col2 = 1.00 WHERE col2 = 35.00 AND col1 = 1.00;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_int;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_droplastcol_addpt_int VALUES(20000,20000,'a',0,20000);

ALTER TABLE mpp21090_pttab_droplastcol_addpt_int DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_droplastcol_addpt_int SELECT 35000,35000,'b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_int SET col1 = 10000 WHERE col2 = 35000 AND col1 = 35000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_int SET col2 = 10000 WHERE col2 = 35000 AND col1 = 10000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_int4;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int,
    col5 int4
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_droplastcol_addpt_int4 VALUES(20000000,20000000,'a',0,20000000);

ALTER TABLE mpp21090_pttab_droplastcol_addpt_int4 DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_droplastcol_addpt_int4 SELECT 35000000,35000000,'b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_int4 SET col1 = 10000000 WHERE col2 = 35000000 AND col1 = 35000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_int4 SET col2 = 10000000 WHERE col2 = 35000000 AND col1 = 10000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_int8;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int,
    col5 int8
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_droplastcol_addpt_int8 VALUES(2000000000000000000,2000000000000000000,'a',0,2000000000000000000);

ALTER TABLE mpp21090_pttab_droplastcol_addpt_int8 DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_droplastcol_addpt_int8 SELECT 3500000000000000000,3500000000000000000,'b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_int8 SET col1 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col1 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col1 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_interval;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int,
    col5 interval
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_interval VALUES('1 hour','1 hour','a',0,'1 hour');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_interval DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_interval SELECT '14 hours','14 hours','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_interval SET col1 = '12 hours' WHERE col2 = '14 hours' AND col1 = '14 hours';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_interval SET col2 = '12 hours' WHERE col2 = '14 hours' AND col1 = '12 hours';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_numeric;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int,
    col5 numeric
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_droplastcol_addpt_numeric VALUES(2.000000,2.000000,'a',0,2.000000);

ALTER TABLE mpp21090_pttab_droplastcol_addpt_numeric DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_numeric SELECT 35.000000,35.000000,'b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_numeric SET col1 = 1.000000 WHERE col2 = 35.000000 AND col1 = 35.000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col1 = 1.000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_text;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int,
    col5 text
    
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0,'abcdefghijklmnop');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_text DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_droplastcol_addpt_text SELECT 'xyz','xyz','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_text ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_text SET col1 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'xyz';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_time;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int,
    col5 time
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_time VALUES('12:00:00','12:00:00','a',0,'12:00:00');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_time DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_droplastcol_addpt_time SELECT '11:30:00','11:30:00','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_time SET col1 = '1:00:00' WHERE col2 = '11:30:00' AND col1 = '11:30:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col1 = '1:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_timestamp;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int,
    col5 timestamp
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0,'2013-12-31 12:00:00');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamp DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamp SET col1 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_droplastcol_addpt_timestamptz;
CREATE TABLE mpp21090_pttab_droplastcol_addpt_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int,
    col5 timestamptz
    
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0,'2013-12-31 12:00:00 PST');

ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamptz DROP COLUMN col5;
ALTER TABLE mpp21090_pttab_droplastcol_addpt_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_droplastcol_addpt_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b',1;
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamptz SET col1 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_droplastcol_addpt_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_droplastcol_addpt_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_droplastcol_addpt_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_char;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_char VALUES('x','x','a','x',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_char DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_dropmidcol_addpt_char SELECT 'z', 'z','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_char SET col1 = '-' WHERE col2 = 'z' AND col1 = 'z';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_char SET col2 = '-' WHERE col2 = 'z' AND col1 = '-';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_date;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_date VALUES('2013-12-31','2013-12-31','a','2013-12-31',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_date DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_dropmidcol_addpt_date SELECT '2014-02-10', '2014-02-10','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_date SET col1 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-02-10';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_decimal;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_decimal VALUES(2.00,2.00,'a',2.00,0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_decimal DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_dropmidcol_addpt_decimal SELECT 35.00, 35.00,'b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_decimal ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_decimal SET col1 = 1.00 WHERE col2 = 35.00 AND col1 = 35.00;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col1 = 1.00;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_float;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_float VALUES(2.00,2.00,'a',2.00,0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_float DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_dropmidcol_addpt_float SELECT 35.00, 35.00,'b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_float ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_float SET col1 = 1.00 WHERE col2 = 35.00 AND col1 = 35.00;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_float SET col2 = 1.00 WHERE col2 = 35.00 AND col1 = 1.00;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_int;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_int VALUES(20000,20000,'a',20000,0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_int DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_dropmidcol_addpt_int SELECT 35000, 35000,'b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_int SET col1 = 10000 WHERE col2 = 35000 AND col1 = 35000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_int SET col2 = 10000 WHERE col2 = 35000 AND col1 = 10000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_int4;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_int4 VALUES(20000000,20000000,'a',20000000,0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_int4 DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_dropmidcol_addpt_int4 SELECT 35000000, 35000000,'b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_int4 SET col1 = 10000000 WHERE col2 = 35000000 AND col1 = 35000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_int4 SET col2 = 10000000 WHERE col2 = 35000000 AND col1 = 10000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_int8;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_int8 VALUES(2000000000000000000,2000000000000000000,'a',2000000000000000000,0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_int8 DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_dropmidcol_addpt_int8 SELECT 3500000000000000000, 3500000000000000000,'b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_int8 SET col1 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col1 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col1 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_interval;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_interval VALUES('1 hour','1 hour','a','1 hour',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_interval DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_dropmidcol_addpt_interval SELECT '14 hours', '14 hours','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_interval SET col1 = '12 hours' WHERE col2 = '14 hours' AND col1 = '14 hours';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_interval SET col2 = '12 hours' WHERE col2 = '14 hours' AND col1 = '12 hours';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_numeric;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_numeric VALUES(2.000000,2.000000,'a',2.000000,0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_numeric DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_dropmidcol_addpt_numeric SELECT 35.000000, 35.000000,'b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_numeric SET col1 = 1.000000 WHERE col2 = 35.000000 AND col1 = 35.000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col1 = 1.000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_text;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 text,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a','abcdefghijklmnop',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_text DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_dropmidcol_addpt_text SELECT 'xyz', 'xyz','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_text SET col1 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'xyz';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_time;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_time VALUES('12:00:00','12:00:00','a','12:00:00',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_time DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_dropmidcol_addpt_time SELECT '11:30:00', '11:30:00','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_time SET col1 = '1:00:00' WHERE col2 = '11:30:00' AND col1 = '11:30:00';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col1 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_timestamp;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a','2013-12-31 12:00:00',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_timestamp DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_dropmidcol_addpt_timestamp SELECT '2014-02-10 12:00:00', '2014-02-10 12:00:00','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_timestamp SET col1 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropmidcol_addpt_timestamptz;
CREATE TABLE mpp21090_pttab_dropmidcol_addpt_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) 
DISTRIBUTED by (col1) 
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_dropmidcol_addpt_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a','2013-12-31 12:00:00 PST',0);

ALTER TABLE mpp21090_pttab_dropmidcol_addpt_timestamptz DROP COLUMN col4;
ALTER TABLE mpp21090_pttab_dropmidcol_addpt_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_dropmidcol_addpt_timestamptz SELECT '2014-02-10 12:00:00 PST', '2014-02-10 12:00:00 PST','b', 1;
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_pttab_dropmidcol_addpt_timestamptz SET col1 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropmidcol_addpt_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropmidcol_addpt_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_dropmidcol_addpt_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_char;
CREATE TABLE mpp21090_reordered_col_dml_char
(
    col1 char DEFAULT 'a',
    col2 char,
    col3 char,
    col4 char,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_reordered_col_dml_char(col2,col1,col3,col5,col4) SELECT 'g', 'g','a', 1,'g';
INSERT INTO mpp21090_reordered_col_dml_char(col2,col3,col5,col4) SELECT 'g','b', 2, 'g'; 
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_char DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_char ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_char(col2,col3,col5,col4) SELECT 'g','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_char SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_char WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_char ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_date;
CREATE TABLE mpp21090_reordered_col_dml_date
(
    col1 date DEFAULT '2014-01-01',
    col2 date,
    col3 char,
    col4 date,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_reordered_col_dml_date(col2,col1,col3,col5,col4) SELECT '2013-12-30', '2013-12-30','a', 1,'2013-12-30';
INSERT INTO mpp21090_reordered_col_dml_date(col2,col3,col5,col4) SELECT '2013-12-30','b', 2, '2013-12-30'; 
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_date DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_date ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_date(col2,col3,col5,col4) SELECT '2013-12-30','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_date SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_date WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_date ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_decimal;
CREATE TABLE mpp21090_reordered_col_dml_decimal
(
    col1 decimal DEFAULT 1.00,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_reordered_col_dml_decimal(col2,col1,col3,col5,col4) SELECT 2.00, 2.00,'a', 1,2.00;
INSERT INTO mpp21090_reordered_col_dml_decimal(col2,col3,col5,col4) SELECT 2.00,'b', 2, 2.00; 
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_decimal DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_decimal ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_decimal(col2,col3,col5,col4) SELECT 2.00,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_decimal SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_decimal WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_decimal ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_float;
CREATE TABLE mpp21090_reordered_col_dml_float
(
    col1 float DEFAULT 1.00,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_reordered_col_dml_float(col2,col1,col3,col5,col4) SELECT 2.00, 2.00,'a', 1,2.00;
INSERT INTO mpp21090_reordered_col_dml_float(col2,col3,col5,col4) SELECT 2.00,'b', 2, 2.00; 
SELECT * FROM mpp21090_reordered_col_dml_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_float DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_float ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_float(col2,col3,col5,col4) SELECT 2.00,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_float ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_float SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_float ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_float WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_float ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_int;
CREATE TABLE mpp21090_reordered_col_dml_int
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_reordered_col_dml_int(col2,col1,col3,col5,col4) SELECT 10000, 10000,'a', 1,10000;
INSERT INTO mpp21090_reordered_col_dml_int(col2,col3,col5,col4) SELECT 10000,'b', 2, 10000; 
SELECT * FROM mpp21090_reordered_col_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_int DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_int ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_int(col2,col3,col5,col4) SELECT 10000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_int ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_int SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_int ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_int WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_int ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_int4;
CREATE TABLE mpp21090_reordered_col_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_reordered_col_dml_int4(col2,col1,col3,col5,col4) SELECT 20000000, 20000000,'a', 1,20000000;
INSERT INTO mpp21090_reordered_col_dml_int4(col2,col3,col5,col4) SELECT 20000000,'b', 2, 20000000; 
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_int4 DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_int4 ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_int4(col2,col3,col5,col4) SELECT 20000000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_int4 SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_int4 WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_int4 ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_int8;
CREATE TABLE mpp21090_reordered_col_dml_int8
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_reordered_col_dml_int8(col2,col1,col3,col5,col4) SELECT 200000000000000000, 200000000000000000,'a', 1,200000000000000000;
INSERT INTO mpp21090_reordered_col_dml_int8(col2,col3,col5,col4) SELECT 200000000000000000,'b', 2, 200000000000000000; 
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_int8 DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_int8 ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_int8(col2,col3,col5,col4) SELECT 200000000000000000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_int8 SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_int8 WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_int8 ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_interval;
CREATE TABLE mpp21090_reordered_col_dml_interval
(
    col1 interval DEFAULT '11 hours',
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_reordered_col_dml_interval(col2,col1,col3,col5,col4) SELECT '10 secs', '10 secs','a', 1,'10 secs';
INSERT INTO mpp21090_reordered_col_dml_interval(col2,col3,col5,col4) SELECT '10 secs','b', 2, '10 secs'; 
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_interval DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_interval ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_interval(col2,col3,col5,col4) SELECT '10 secs','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_interval SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_interval WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_interval ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_numeric;
CREATE TABLE mpp21090_reordered_col_dml_numeric
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_reordered_col_dml_numeric(col2,col1,col3,col5,col4) SELECT 2.000000, 2.000000,'a', 1,2.000000;
INSERT INTO mpp21090_reordered_col_dml_numeric(col2,col3,col5,col4) SELECT 2.000000,'b', 2, 2.000000; 
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_numeric DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_numeric ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_numeric(col2,col3,col5,col4) SELECT 2.000000,'c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_numeric SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_numeric WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_numeric ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_time;
CREATE TABLE mpp21090_reordered_col_dml_time
(
    col1 time DEFAULT '1:00:00',
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_reordered_col_dml_time(col2,col1,col3,col5,col4) SELECT '12:00:00', '12:00:00','a', 1,'12:00:00';
INSERT INTO mpp21090_reordered_col_dml_time(col2,col3,col5,col4) SELECT '12:00:00','b', 2, '12:00:00'; 
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_time DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_time ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_time(col2,col3,col5,col4) SELECT '12:00:00','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_time SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_time WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_time ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_timestamp;
CREATE TABLE mpp21090_reordered_col_dml_timestamp
(
    col1 timestamp DEFAULT '2014-01-01 12:00:00',
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_reordered_col_dml_timestamp(col2,col1,col3,col5,col4) SELECT '2013-12-30 12:00:00', '2013-12-30 12:00:00','a', 1,'2013-12-30 12:00:00';
INSERT INTO mpp21090_reordered_col_dml_timestamp(col2,col3,col5,col4) SELECT '2013-12-30 12:00:00','b', 2, '2013-12-30 12:00:00'; 
SELECT * FROM mpp21090_reordered_col_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_timestamp DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_timestamp ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_timestamp(col2,col3,col5,col4) SELECT '2013-12-30 12:00:00','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_timestamp ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_timestamp SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_timestamp ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_timestamp WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_timestamp ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_reordered_col_dml_timestamptz;
CREATE TABLE mpp21090_reordered_col_dml_timestamptz
(
    col1 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_reordered_col_dml_timestamptz(col2,col1,col3,col5,col4) SELECT '2013-12-30 12:00:00 PST', '2013-12-30 12:00:00 PST','a', 1,'2013-12-30 12:00:00 PST';
INSERT INTO mpp21090_reordered_col_dml_timestamptz(col2,col3,col5,col4) SELECT '2013-12-30 12:00:00 PST','b', 2, '2013-12-30 12:00:00 PST'; 
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_reordered_col_dml_timestamptz DROP COLUMN col4;
ALTER TABLE mpp21090_reordered_col_dml_timestamptz ADD COLUMN col4 int DEFAULT 10;

INSERT INTO mpp21090_reordered_col_dml_timestamptz(col2,col3,col5,col4) SELECT '2013-12-30 12:00:00 PST','c', 2, 10; 
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

UPDATE mpp21090_reordered_col_dml_timestamptz SET col4 = 20;
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

DELETE FROM mpp21090_reordered_col_dml_timestamptz WHERE col4=20;
SELECT * FROM mpp21090_reordered_col_dml_timestamptz ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_char;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int,
    col5 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_char VALUES('g','g','a',0, 'g');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_char DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_char ADD COLUMN col1 char DEFAULT 'g';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_char) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate VALUES('g','z',1,'g','g');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_char EXCHANGE PARTITION FOR('d') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_char SELECT  'a','b', 1, 'a', 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_char SET col5 = 'z' WHERE col2 = 'a' AND col5 = 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_char SET col2 ='g' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate SELECT 'a','b', 1, 'a', 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate SET col2='g' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_char_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_date;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int,
    col5 date
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date VALUES('2013-12-30','2013-12-30','a',0, '2013-12-30');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date ADD COLUMN col1 date DEFAULT '2013-12-30';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_date) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate VALUES('2013-12-30','z',1,'2013-12-30','2013-12-30');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date EXCHANGE PARTITION FOR('2013-12-15') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date SELECT  '2013-12-01','b', 1, '2013-12-01', '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_date SET col5 = '2014-02-10' WHERE col2 = '2013-12-01' AND col5 = '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_date SET col2 ='2013-12-30' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate SELECT '2013-12-01','b', 1, '2013-12-01', '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate SET col2='2013-12-30' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_date_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_decimal;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int,
    col5 decimal
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal VALUES(2.00,2.00,'a',0, 2.00);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ADD COLUMN col1 decimal DEFAULT 2.00;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_decimal) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate VALUES(2.00,'z',1,2.00,2.00);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal EXCHANGE PARTITION FOR(5.00) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal SELECT  1.00,'b', 1, 1.00, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal SET col5 = 35.00 WHERE col2 = 1.00 AND col5 = 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal SET col2 =2.00 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate SELECT 1.00,'b', 1, 1.00, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate SET col2=2.00 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_decimal_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_float;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int,
    col5 float
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_float VALUES(2.00,2.00,'a',0, 2.00);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_float DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_float ADD COLUMN col1 float DEFAULT 2.00;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_float) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate VALUES(2.00,'z',1,2.00,2.00);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_float EXCHANGE PARTITION FOR(5.00) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_float SELECT  1.00,'b', 1, 1.00, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_float SET col5 = 35.00 WHERE col2 = 1.00 AND col5 = 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_float SET col2 =2.00 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate SELECT 1.00,'b', 1, 1.00, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate SET col2=2.00 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_float_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int VALUES(10000,10000,'a',0, 10000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int ADD COLUMN col1 int DEFAULT 10000;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_int) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate VALUES(10000,'z',1,10000,10000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int EXCHANGE PARTITION FOR(5000) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int SELECT  1000,'b', 1, 1000, 1000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int SET col5 = 35000 WHERE col2 = 1000 AND col5 = 1000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int SET col2 =10000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate SELECT 1000,'b', 1, 1000, 1000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate SET col2=10000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int4;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int,
    col5 int4
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int4 VALUES(20000000,20000000,'a',0, 20000000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int4 DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ADD COLUMN col1 int4 DEFAULT 20000000;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_int4) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate VALUES(20000000,'z',1,20000000,20000000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int4 EXCHANGE PARTITION FOR(50000000) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int4 SELECT  10000000,'b', 1, 10000000, 10000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int4 SET col5 = 350000000 WHERE col2 = 10000000 AND col5 = 10000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int4 SET col2 =20000000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate SELECT 10000000,'b', 1, 10000000, 10000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate SET col2=20000000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int4_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int8;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int,
    col5 int8
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8 VALUES(200000000000000000,200000000000000000,'a',0, 200000000000000000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ADD COLUMN col1 int8 DEFAULT 200000000000000000;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_int8) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate VALUES(200000000000000000,'z',1,200000000000000000,200000000000000000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 EXCHANGE PARTITION FOR(500000000000000000) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8 SELECT  100000000000000000,'b', 1, 100000000000000000, 100000000000000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 SET col5 = 3500000000000000000 WHERE col2 = 100000000000000000 AND col5 = 100000000000000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int8 SET col2 =200000000000000000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate SELECT 100000000000000000,'b', 1, 100000000000000000, 100000000000000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate SET col2=200000000000000000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_int8_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_interval;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int,
    col5 interval
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_interval VALUES('10 secs','10 secs','a',0, '10 secs');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_interval DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_interval ADD COLUMN col1 interval DEFAULT '10 secs';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_interval) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate VALUES('10 secs','z',1,'10 secs','10 secs');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_interval EXCHANGE PARTITION FOR('30 secs') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_interval SELECT  '1 sec','b', 1, '1 sec', '1 sec';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_interval SET col5 = '14 hours' WHERE col2 = '1 sec' AND col5 = '1 sec';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_interval SET col2 ='10 secs' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate SELECT '1 sec','b', 1, '1 sec', '1 sec';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate SET col2='10 secs' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_interval_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_numeric;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int,
    col5 numeric
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_numeric VALUES(2.000000,2.000000,'a',0, 2.000000);
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ADD COLUMN col1 numeric DEFAULT 2.000000;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_numeric) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate VALUES(2.000000,'z',1,2.000000,2.000000);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric EXCHANGE PARTITION FOR(5.000000) WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_numeric SELECT  1.000000,'b', 1, 1.000000, 1.000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric SET col5 = 35.000000 WHERE col2 = 1.000000 AND col5 = 1.000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric SET col2 =2.000000 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate SELECT 1.000000,'b', 1, 1.000000, 1.000000;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate SET col2=2.000000 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_numeric_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_time;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int,
    col5 time
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_time VALUES('12:00:00','12:00:00','a',0, '12:00:00');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_time DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_time ADD COLUMN col1 time DEFAULT '12:00:00';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_time) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate VALUES('12:00:00','z',1,'12:00:00','12:00:00');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_time EXCHANGE PARTITION FOR('15:00:00') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_time SELECT  '12:00:00','b', 1, '12:00:00', '12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_time SET col5 = '11:30:00' WHERE col2 = '12:00:00' AND col5 = '12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_time SET col2 ='12:00:00' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate SELECT '12:00:00','b', 1, '12:00:00', '12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate SET col2='12:00:00' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_time_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int,
    col5 timestamp
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a',0, '2013-12-30 12:00:00');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ADD COLUMN col1 timestamp DEFAULT '2013-12-30 12:00:00';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate VALUES('2013-12-30 12:00:00','z',1,'2013-12-30 12:00:00','2013-12-30 12:00:00');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp EXCHANGE PARTITION FOR('2013-12-15 12:00:00') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp SELECT  '2013-12-01 12:00:00','b', 1, '2013-12-01 12:00:00', '2013-12-01 12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp SET col5 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col5 = '2013-12-01 12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp SET col2 ='2013-12-30 12:00:00' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate SELECT '2013-12-01 12:00:00','b', 1, '2013-12-01 12:00:00', '2013-12-01 12:00:00';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate SET col2='2013-12-30 12:00:00' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamp_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int,
    col5 timestamptz
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a',0, '2013-12-30 12:00:00 PST');
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz DROP COLUMN col1;
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ADD COLUMN col1 timestamptz DEFAULT '2013-12-30 12:00:00 PST';

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate( like mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate VALUES('2013-12-30 12:00:00 PST','z',1,'2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz EXCHANGE PARTITION FOR('2013-12-15 12:00:00 PST') WITH TABLE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz SELECT  '2013-12-01 12:00:00 PST','b', 1, '2013-12-01 12:00:00 PST', '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz SET col5 = '2014-02-10 12:00:00 PST' WHERE col2 = '2013-12-01 12:00:00 PST' AND col5 = '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz SET col2 ='2013-12-30 12:00:00 PST' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate SELECT '2013-12-01 12:00:00 PST','b', 1, '2013-12-01 12:00:00 PST', '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate SET col2='2013-12-30 12:00:00 PST' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_addcol_dml_timestamptz_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_char;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int,
    col5 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char VALUES('g','g','a',0, 'g');
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_char DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_char_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_char_candidate( like mpp21090_xchange_pttab_dropcol_dml_char) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char_candidate VALUES('g','z',1,'g');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_char EXCHANGE PARTITION FOR('d') WITH TABLE mpp21090_xchange_pttab_dropcol_dml_char_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char SELECT  'a','b', 1, 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_char SET col5 = 'z' WHERE col2 = 'a' AND col5 = 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_char SET col2 ='g' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_char WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_char_candidate SELECT 'a','b', 1, 'a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_char_candidate SET col2='g' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_char_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_date;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int,
    col5 date
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_date VALUES('2013-12-30','2013-12-30','a',0, '2013-12-30');
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_date DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_date_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_date_candidate( like mpp21090_xchange_pttab_dropcol_dml_date) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_date_candidate VALUES('2013-12-30','z',1,'2013-12-30');

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_date EXCHANGE PARTITION FOR('2013-12-15') WITH TABLE mpp21090_xchange_pttab_dropcol_dml_date_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_date SELECT  '2013-12-01','b', 1, '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_date SET col5 = '2014-02-10' WHERE col2 = '2013-12-01' AND col5 = '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_date SET col2 ='2013-12-30' WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_date WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_date_candidate SELECT '2013-12-01','b', 1, '2013-12-01';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_date_candidate SET col2='2013-12-30' WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_date_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_date_candidate ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_decimal;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int,
    col5 decimal
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_xchange_pttab_dropcol_dml_decimal VALUES(2.00,2.00,'a',0, 2.00);
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_decimal DROP COLUMN col1;

-- Create Candidate table for Exchange
DROP TABLE IF EXISTS mpp21090_xchange_pttab_dropcol_dml_decimal_candidate;
CREATE TABLE mpp21090_xchange_pttab_dropcol_dml_decimal_candidate( like mpp21090_xchange_pttab_dropcol_dml_decimal) distributed randomly;
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_decimal_candidate VALUES(2.00,'z',1,2.00);

-- Exchange 
ALTER TABLE mpp21090_xchange_pttab_dropcol_dml_decimal EXCHANGE PARTITION FOR(5.00) WITH TABLE mpp21090_xchange_pttab_dropcol_dml_decimal_candidate;

SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal ORDER BY 1,2,3;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal_candidate ORDER BY 1,2,3;

-- DML on partition table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_decimal SELECT  1.00,'b', 1, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_decimal SET col5 = 35.00 WHERE col2 = 1.00 AND col5 = 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_decimal SET col2 =2.00 WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_decimal WHERE col3='b';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal ORDER BY 1,2,3;

-- DML on candidate table
INSERT INTO mpp21090_xchange_pttab_dropcol_dml_decimal_candidate SELECT 1.00,'b', 1, 1.00;
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal_candidate ORDER BY 1,2,3;

UPDATE mpp21090_xchange_pttab_dropcol_dml_decimal_candidate SET col2=2.00 WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal_candidate ORDER BY 1,2,3;

DELETE FROM mpp21090_xchange_pttab_dropcol_dml_decimal_candidate WHERE col3='a';
SELECT * FROM mpp21090_xchange_pttab_dropcol_dml_decimal_candidate ORDER BY 1,2,3;

-- start_ignore
drop schema sql_partition_451_500 cascade;
-- end_ignore
