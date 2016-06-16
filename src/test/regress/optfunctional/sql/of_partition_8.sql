
-- start_ignore
create schema sql_partition_351_400;
set search_path to sql_partition_351_400;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_int;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_int;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_int on mpp21090_dropcol_splitpt_idx_dml_int(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int VALUES(10000,10000,'a',10000,0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int SPLIT PARTITION partone at (5000) into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int SELECT 1000, 1000,'b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int SET col1 = 35000 WHERE col2 = 1000 AND col1 = 1000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int SET col2 =10000  WHERE col2 = 1000 AND col1 = 35000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_int WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_int4;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_int4;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_int4 on mpp21090_dropcol_splitpt_idx_dml_int4(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int4 VALUES(20000000,20000000,'a',20000000,0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int4 DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int4 SPLIT PARTITION partone at (50000000) into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int4 SELECT 10000000, 10000000,'b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int4 SET col1 = 350000000 WHERE col2 = 10000000 AND col1 = 10000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int4 SET col2 =20000000  WHERE col2 = 10000000 AND col1 = 350000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_int4 WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int4 ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_int8;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_int8;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_int8 on mpp21090_dropcol_splitpt_idx_dml_int8(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int8 DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_int8 SPLIT PARTITION partone at (500000000000000000) into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_int8 SELECT 100000000000000000, 100000000000000000,'b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int8 SET col1 = 3500000000000000000 WHERE col2 = 100000000000000000 AND col1 = 100000000000000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_int8 SET col2 =200000000000000000  WHERE col2 = 100000000000000000 AND col1 = 3500000000000000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_int8 WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_int8 ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_interval;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_interval;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_interval on mpp21090_dropcol_splitpt_idx_dml_interval(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_interval VALUES('10 secs','10 secs','a','10 secs',0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_interval DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_interval SPLIT PARTITION partone at ('30 secs') into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_interval SELECT '1 sec', '1 sec','b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_interval SET col1 = '14 hours' WHERE col2 = '1 sec' AND col1 = '1 sec';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_interval SET col2 ='10 secs'  WHERE col2 = '1 sec' AND col1 = '14 hours';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_interval WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_interval ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_numeric;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_numeric;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_numeric on mpp21090_dropcol_splitpt_idx_dml_numeric(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_numeric DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_numeric SPLIT PARTITION partone at (5.000000) into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_numeric SELECT 1.000000, 1.000000,'b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_numeric SET col1 = 35.000000 WHERE col2 = 1.000000 AND col1 = 1.000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_numeric SET col2 =2.000000  WHERE col2 = 1.000000 AND col1 = 35.000000;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_numeric WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_numeric ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_time;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_time;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_time on mpp21090_dropcol_splitpt_idx_dml_time(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_time VALUES('12:00:00','12:00:00','a','12:00:00',0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_time DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_time SPLIT PARTITION partone at ('15:00:00') into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_time SELECT '12:00:00', '12:00:00','b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_time SET col1 = '11:30:00' WHERE col2 = '12:00:00' AND col1 = '12:00:00';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_time SET col2 ='12:00:00'  WHERE col2 = '12:00:00' AND col1 = '11:30:00';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_time WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_time ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_timestamp;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_timestamp;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_timestamp on mpp21090_dropcol_splitpt_idx_dml_timestamp(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a','2013-12-30 12:00:00',0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_timestamp DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_timestamp SPLIT PARTITION partone at ('2013-12-15 12:00:00') into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_timestamp SELECT '2013-12-01 12:00:00', '2013-12-01 12:00:00','b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_timestamp SET col1 = '2014-02-10 12:00:00' WHERE col2 = '2013-12-01 12:00:00' AND col1 = '2013-12-01 12:00:00';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_timestamp SET col2 ='2013-12-30 12:00:00'  WHERE col2 = '2013-12-01 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_timestamp WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamp ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_dropcol_splitpt_idx_dml_timestamptz;
CREATE TABLE mpp21090_dropcol_splitpt_idx_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

DROP INDEX IF EXISTS mpp21090_dropcol_splitpt_idx_dml_idx_timestamptz;
CREATE INDEX mpp21090_dropcol_splitpt_idx_dml_idx_timestamptz on mpp21090_dropcol_splitpt_idx_dml_timestamptz(col2);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a','2013-12-30 12:00:00 PST',0);
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_timestamptz DROP COLUMN col4;

ALTER TABLE mpp21090_dropcol_splitpt_idx_dml_timestamptz SPLIT PARTITION partone at ('2013-12-15 12:00:00 PST') into (partition partsplitone,partition partsplitwo);

INSERT INTO mpp21090_dropcol_splitpt_idx_dml_timestamptz SELECT '2013-12-01 12:00:00 PST', '2013-12-01 12:00:00 PST','b', 1;
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE mpp21090_dropcol_splitpt_idx_dml_timestamptz SET col1 = '2014-02-10 12:00:00 PST' WHERE col2 = '2013-12-01 12:00:00 PST' AND col1 = '2013-12-01 12:00:00 PST';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_dropcol_splitpt_idx_dml_timestamptz SET col2 ='2013-12-30 12:00:00 PST'  WHERE col2 = '2013-12-01 12:00:00 PST' AND col1 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_dropcol_splitpt_idx_dml_timestamptz WHERE col3='b';
SELECT * FROM mpp21090_dropcol_splitpt_idx_dml_timestamptz ORDER BY 1,2,3;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_char;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_char VALUES('x','x','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_char ADD COLUMN col5 char DEFAULT 'x';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_char SELECT 'z','z','b',1, 'z';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_char DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_char SELECT 'z','c',1, 'z';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_char SET col5 = '-' WHERE col2 = 'z' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_char SET col2 = '-' WHERE col2 = 'z' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_date;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_date VALUES('2013-12-31','2013-12-31','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_date ADD COLUMN col5 date DEFAULT '2013-12-31';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_date SELECT '2014-02-10','2014-02-10','b',1, '2014-02-10';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_date DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_date SELECT '2014-02-10','c',1, '2014-02-10';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_date SET col5 = '2014-01-01' WHERE col2 = '2014-02-10' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_decimal;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal ADD COLUMN col5 decimal DEFAULT 2.00;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_decimal SELECT 35.00,35.00,'b',1, 35.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_decimal DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_decimal SELECT 35.00,'c',1, 35.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_decimal SET col5 = 1.00 WHERE col2 = 35.00 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_decimal SET col2 = 1.00 WHERE col2 = 35.00 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_float;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_float VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_float ADD COLUMN col5 float DEFAULT 2.00;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_float SELECT 35.00,35.00,'b',1, 35.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_float DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_float SELECT 35.00,'c',1, 35.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_float SET col5 = 1.00 WHERE col2 = 35.00 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_float SET col2 = 1.00 WHERE col2 = 35.00 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_int;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int VALUES(20000,20000,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int ADD COLUMN col5 int DEFAULT 20000;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int SELECT 35000,35000,'b',1, 35000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int SELECT 35000,'c',1, 35000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_int SET col5 = 10000 WHERE col2 = 35000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_int SET col2 = 10000 WHERE col2 = 35000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_int4;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int4 VALUES(20000000,20000000,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int4 ADD COLUMN col5 int4 DEFAULT 20000000;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int4 SELECT 35000000,35000000,'b',1, 35000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int4 DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int4 SELECT 35000000,'c',1, 35000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_int4 SET col5 = 10000000 WHERE col2 = 35000000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_int4 SET col2 = 10000000 WHERE col2 = 35000000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_int8;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int8 VALUES(2000000000000000000,2000000000000000000,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int8 ADD COLUMN col5 int8 DEFAULT 2000000000000000000;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int8 SELECT 3500000000000000000,3500000000000000000,'b',1, 3500000000000000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_int8 DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_int8 SELECT 3500000000000000000,'c',1, 3500000000000000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_int8 SET col5 = 1000000000000000000 WHERE col2 = 3500000000000000000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_interval;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_interval VALUES('1 hour','1 hour','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_interval ADD COLUMN col5 interval DEFAULT '1 hour';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_interval SELECT '14 hours','14 hours','b',1, '14 hours';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_interval DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_interval SELECT '14 hours','c',1, '14 hours';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_interval SET col5 = '12 hours' WHERE col2 = '14 hours' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_interval SET col2 = '12 hours' WHERE col2 = '14 hours' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_numeric;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_numeric ADD COLUMN col5 numeric DEFAULT 2.000000;
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_numeric SELECT 35.000000,35.000000,'b',1, 35.000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_numeric DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_numeric SELECT 35.000000,'c',1, 35.000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_numeric SET col5 = 1.000000 WHERE col2 = 35.000000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_text;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_text ADD COLUMN col5 text DEFAULT 'abcdefghijklmnop';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_text SELECT 'xyz','xyz','b',1, 'xyz';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_text DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_text SELECT 'xyz','c',1, 'xyz';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_text SET col5 = 'qrstuvwxyz' WHERE col2 = 'xyz' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_time;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_time ADD COLUMN col5 time DEFAULT '12:00:00';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_time SELECT '11:30:00','11:30:00','b',1, '11:30:00';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_time DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_time SELECT '11:30:00','c',1, '11:30:00';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_time SET col5 = '1:00:00' WHERE col2 = '11:30:00' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_timestamp;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamp ADD COLUMN col5 timestamp DEFAULT '2013-12-31 12:00:00';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','b',1, '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamp DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamp SELECT '2014-02-10 12:00:00','c',1, '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_timestamp SET col5 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addcol_addpt_dropcol_timestamptz;
CREATE TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0);

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz ADD COLUMN col5 timestamptz DEFAULT '2013-12-31 12:00:00 PST';
ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b',1, '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addcol_addpt_dropcol_timestamptz DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addcol_addpt_dropcol_timestamptz SELECT '2014-02-10 12:00:00 PST','c',1, '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addcol_addpt_dropcol_timestamptz SET col5 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addcol_addpt_dropcol_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' and col3='c';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addcol_addpt_dropcol_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_char;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_char VALUES('x','x','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_char SELECT 'z','z','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_char DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_char SELECT 'z','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_char ADD COLUMN col5 char DEFAULT 'x';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_char SELECT 'z','d',1,'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_char SET col4 = 10 WHERE col2 = 'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_char SET col2 = '-' WHERE col2 = 'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_date;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_date VALUES('2013-12-31','2013-12-31','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_date SELECT '2014-02-10','2014-02-10','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_date DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_date SELECT '2014-02-10','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_date ADD COLUMN col5 date DEFAULT '2013-12-31';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_date SELECT '2014-02-10','d',1,'2014-02-10';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_date SET col4 = 10 WHERE col2 = '2014-02-10';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_decimal;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_decimal SELECT 35.00,35.00,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_decimal DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_decimal SELECT 35.00,'c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ADD COLUMN col5 decimal DEFAULT 2.00;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_decimal SELECT 35.00,'d',1,35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_decimal SET col4 = 10 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_decimal SET col2 = 1.00 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_float;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_float VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_float SELECT 35.00,35.00,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_float DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_float SELECT 35.00,'c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_float ADD COLUMN col5 float DEFAULT 2.00;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_float SELECT 35.00,'d',1,35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_float SET col4 = 10 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_float SET col2 = 1.00 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_int;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int VALUES(20000,20000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int SELECT 35000,35000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int SELECT 35000,'c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int ADD COLUMN col5 int DEFAULT 20000;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int SELECT 35000,'d',1,35000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_int SET col4 = 10 WHERE col2 = 35000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_int SET col2 = 10000 WHERE col2 = 35000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_int4;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int4 VALUES(20000000,20000000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int4 SELECT 35000000,35000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int4 DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int4 SELECT 35000000,'c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ADD COLUMN col5 int4 DEFAULT 20000000;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int4 SELECT 35000000,'d',1,35000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_int4 SET col4 = 10 WHERE col2 = 35000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_int4 SET col2 = 10000000 WHERE col2 = 35000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_int8;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int8 VALUES(2000000000000000000,2000000000000000000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int8 SELECT 3500000000000000000,3500000000000000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int8 DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int8 SELECT 3500000000000000000,'c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ADD COLUMN col5 int8 DEFAULT 2000000000000000000;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_int8 SELECT 3500000000000000000,'d',1,3500000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_int8 SET col4 = 10 WHERE col2 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_interval;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_interval VALUES('1 hour','1 hour','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_interval SELECT '14 hours','14 hours','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_interval DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_interval SELECT '14 hours','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_interval ADD COLUMN col5 interval DEFAULT '1 hour';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_interval SELECT '14 hours','d',1,'14 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_interval SET col4 = 10 WHERE col2 = '14 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_interval SET col2 = '12 hours' WHERE col2 = '14 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_numeric;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_numeric SELECT 35.000000,35.000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_numeric DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_numeric SELECT 35.000000,'c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ADD COLUMN col5 numeric DEFAULT 2.000000;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_numeric SELECT 35.000000,'d',1,35.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_numeric SET col4 = 10 WHERE col2 = 35.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_numeric SET col2 = 1.000000 WHERE col2 = 35.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_text;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text SELECT 'xyz','xyz','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text SELECT 'xyz','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_text ADD COLUMN col5 text DEFAULT 'abcdefghijklmnop';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_text SELECT 'xyz','d',1,'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_text SET col4 = 10 WHERE col2 = 'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_time;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_time SELECT '11:30:00','11:30:00','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_time DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_time SELECT '11:30:00','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_time ADD COLUMN col5 time DEFAULT '12:00:00';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_time SELECT '11:30:00','d',1,'11:30:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_time SET col4 = 10 WHERE col2 = '11:30:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_time SET col2 = '1:00:00' WHERE col2 = '11:30:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp SELECT '2014-02-10 12:00:00','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ADD COLUMN col5 timestamp DEFAULT '2013-12-31 12:00:00';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp SELECT '2014-02-10 12:00:00','d',1,'2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp SET col4 = 10 WHERE col2 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz;
CREATE TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','c',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ADD COLUMN col5 timestamptz DEFAULT '2013-12-31 12:00:00 PST';

INSERT INTO mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','d',1,'2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz SET col4 = 10 WHERE col2 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_char;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_char VALUES('x','x','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_char ADD PARTITION partfour VALUES('y','z','-');

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_char SELECT 'z','z','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_char DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_char SELECT 'z','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_char SET col4 = 10 WHERE col2 = 'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_char SET col2 = '-' WHERE col2 = 'z';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_char WHERE col2 = '-';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_date;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_date VALUES('2013-12-31','2013-12-31','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_date SELECT '2014-02-10','2014-02-10','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_date DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_date SELECT '2014-02-10','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_date SET col4 = 10 WHERE col2 = '2014-02-10';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_decimal;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_decimal SELECT 35.00,35.00,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_decimal DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_decimal SELECT 35.00,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_decimal SET col4 = 10 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_decimal SET col2 = 1.00 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_float;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_float VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_float ADD PARTITION partfour start(30.00) end(40.00);

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_float SELECT 35.00,35.00,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_float DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_float SELECT 35.00,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_float SET col4 = 10 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_float SET col2 = 1.00 WHERE col2 = 35.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_int;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int VALUES(20000,20000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_int ADD PARTITION partfour start(30001) end(40001);

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int SELECT 35000,35000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_int DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int SELECT 35000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_int SET col4 = 10 WHERE col2 = 35000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_int SET col2 = 10000 WHERE col2 = 35000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_int WHERE col2 = 10000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_int4;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int4 VALUES(20000000,20000000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_int4 ADD PARTITION partfour start(300000001) end(400000001);

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int4 SELECT 35000000,35000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_int4 DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int4 SELECT 35000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_int4 SET col4 = 10 WHERE col2 = 35000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_int4 SET col2 = 10000000 WHERE col2 = 35000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_int8;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int8 VALUES(2000000000000000000,2000000000000000000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_int8 ADD PARTITION partfour start(3000000000000000001) end(4000000000000000001);

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int8 SELECT 3500000000000000000,3500000000000000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_int8 DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_int8 SELECT 3500000000000000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_int8 SET col4 = 10 WHERE col2 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_interval;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_interval VALUES('1 hour','1 hour','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_interval ADD PARTITION partfour start('12 hours') end('1 day') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_interval SELECT '14 hours','14 hours','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_interval DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_interval SELECT '14 hours','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_interval SET col4 = 10 WHERE col2 = '14 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_interval SET col2 = '12 hours' WHERE col2 = '14 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_numeric;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_numeric ADD PARTITION partfour start(30.000000) end(40.000000) inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_numeric SELECT 35.000000,35.000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_numeric DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_numeric SELECT 35.000000,'b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_numeric SET col4 = 10 WHERE col2 = 35.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_numeric SET col2 = 1.000000 WHERE col2 = 35.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_text;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('abcdefghijklmnop') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('qrstuvwxyz') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('ghi'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_text ADD PARTITION partfour VALUES('xyz');

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_text SELECT 'xyz','xyz','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_text DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_text SELECT 'xyz','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_text ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_text SET col4 = 10 WHERE col2 = 'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_text ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_time;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_time ADD PARTITION partfour start('11:00:00') end('12:00:00');

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_time SELECT '11:30:00','11:30:00','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_time DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_time SELECT '11:30:00','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_time SET col4 = 10 WHERE col2 = '11:30:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_time SET col2 = '1:00:00' WHERE col2 = '11:30:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_timestamp;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_timestamp ADD PARTITION partfour start('2014-02-01 12:00:00') end('2014-03-01 12:00:00') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_timestamp DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_timestamp SELECT '2014-02-10 12:00:00','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_timestamp SET col4 = 10 WHERE col2 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_addpt_dropcol_dml_timestamptz;
CREATE TABLE mpp21090_pttab_addpt_dropcol_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0);

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_timestamptz ADD PARTITION partfour start('2014-02-01 12:00:00 PST') end('2014-03-01 12:00:00 PST') inclusive;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_addpt_dropcol_dml_timestamptz DROP COLUMN col1;

INSERT INTO mpp21090_pttab_addpt_dropcol_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','b',1;
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_pttab_addpt_dropcol_dml_timestamptz SET col4 = 10 WHERE col2 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_addpt_dropcol_dml_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_addpt_dropcol_dml_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_pttab_addpt_dropcol_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_char;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_char VALUES('x','x','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_char DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_char VALUES('x','x','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_char ADD COLUMN col5 char DEFAULT 'x';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_char SELECT 'x','x','c','x';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_char SET col5 = '-' WHERE col2 = 'x' AND col1 = 'x';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char WHERE col5 = '-';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_char ADD PARTITION partfour VALUES('y','z','-');
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_char ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_char SELECT 'z','z','d','z';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_char SET col5 = '-' WHERE col2 = 'z' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_char SET col2 = '-' WHERE col2 = 'z' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char WHERE col5 = '-';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_date;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_date VALUES('2013-12-31','2013-12-31','a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_date DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_date VALUES('2013-12-31','2013-12-31','b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_date ADD COLUMN col5 date DEFAULT '2013-12-31';

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_date SELECT '2013-12-31','2013-12-31','c','2013-12-31';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_date SET col5 = '2014-01-01' WHERE col2 = '2013-12-31' AND col1 = '2013-12-31';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date WHERE col5 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_date ADD PARTITION partfour start('2014-02-01') end('2014-03-01') inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_date ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_date SELECT '2014-02-10','2014-02-10','d','2014-02-10';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_date SET col5 = '2014-01-01' WHERE col2 = '2014-02-10' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date WHERE col5 = '2014-01-01';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_pttab_dropcol_addcol_addpt_dml_decimal;
CREATE TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal DROP COLUMN col4;
INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_decimal VALUES(2.00,2.00,'b');

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ADD COLUMN col5 decimal DEFAULT 2.00;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_decimal SELECT 2.00,2.00,'c',2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal SET col5 = 1.00 WHERE col2 = 2.00 AND col1 = 2.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ADD PARTITION partfour start(30.00) end(40.00) inclusive;
ALTER TABLE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ADD DEFAULT partition def;

INSERT INTO mpp21090_pttab_dropcol_addcol_addpt_dml_decimal SELECT 35.00,35.00,'d',35.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal SET col5 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_pttab_dropcol_addcol_addpt_dml_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col3 ='d';
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal WHERE col5 = 1.00;
SELECT * FROM mpp21090_pttab_dropcol_addcol_addpt_dml_decimal ORDER BY 1,2,3;


-- start_ignore
drop schema sql_partition_351_400 cascade;
-- end_ignore
