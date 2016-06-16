
-- start_ignore
create schema sql_partition_1_50;
set search_path to sql_partition_1_50;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_char;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_char VALUES('g','g','a','g',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_char DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_char VALUES('g','g','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_char SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_char SELECT 'a', 'a','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_char SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_char WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_date;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_date VALUES('2013-12-30','2013-12-30','a','2013-12-30',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_date DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_date VALUES('2013-12-30','2013-12-30','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_date SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_date SELECT '2014-01-01', '2014-01-01','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_date SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_date WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_decimal;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_decimal VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_decimal DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_decimal VALUES(2.00,2.00,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_decimal SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_decimal SELECT 1.00, 1.00,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_decimal SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_decimal WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_float;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_float VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_float DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_float VALUES(2.00,2.00,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_float ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_float SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_float SELECT 1.00, 1.00,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_float ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_float SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_float ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_float WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_int;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_int VALUES(10000,10000,'a',10000,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int VALUES(10000,10000,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int SELECT 20000, 20000,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_int SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_int WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_int4;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_int4 VALUES(20000000,20000000,'a',20000000,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int4 DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int4 VALUES(20000000,20000000,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int4 SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int4 SELECT 10000000, 10000000,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_int4 SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_int4 WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_int8;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int8 DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int8 VALUES(200000000000000000,200000000000000000,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_int8 SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_int8 SELECT 1000000000000000000, 1000000000000000000,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_int8 SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_int8 WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_interval;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_interval VALUES('10 secs','10 secs','a','10 secs',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_interval DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_interval VALUES('10 secs','10 secs','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_interval SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_interval SELECT '11 hours', '11 hours','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_interval SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_interval WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_numeric;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_numeric DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_numeric VALUES(2.000000,2.000000,'b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_numeric SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_numeric SELECT 1.000000, 1.000000,'c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_numeric SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_numeric WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_time;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_time VALUES('12:00:00','12:00:00','a','12:00:00',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_time DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_time VALUES('12:00:00','12:00:00','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_time ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_time SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_time SELECT '1:00:00', '1:00:00','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_time ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_time SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_time ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_time WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_timestamp;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a','2013-12-30 12:00:00',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_timestamp DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_timestamp SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_timestamp SELECT '2014-01-01 12:00:00', '2014-01-01 12:00:00','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_timestamp SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_timestamp WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_changedistpolicy_dml_pttab_timestamptz;
CREATE TABLE mpp21090_changedistpolicy_dml_pttab_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));


INSERT INTO mpp21090_changedistpolicy_dml_pttab_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a','2013-12-30 12:00:00 PST',0);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_timestamptz DROP COLUMN col4;

INSERT INTO mpp21090_changedistpolicy_dml_pttab_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','b',1);
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE mpp21090_changedistpolicy_dml_pttab_timestamptz SET DISTRIBUTED BY (col3);

INSERT INTO mpp21090_changedistpolicy_dml_pttab_timestamptz SELECT '2014-01-01 12:00:00 PST', '2014-01-01 12:00:00 PST','c', 2;
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_changedistpolicy_dml_pttab_timestamptz SET col3 ='c' WHERE col3 ='b';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_changedistpolicy_dml_pttab_timestamptz WHERE col3 ='c';
SELECT * FROM mpp21090_changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_char;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_char VALUES('x','x','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_char DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_char SELECT 'z','z','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_char ADD COLUMN col5 char;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_char SELECT 'x','x','c','x';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_char SET col1 = '-' WHERE col2 = 'z' AND col1 = 'z';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_char SET col2 = '-' WHERE col2 = 'z' AND col1 = '-';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_char WHERE col2 = '-';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_date;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_date VALUES('2013-12-31','2013-12-31','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_date DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_date SELECT '2014-02-10','2014-02-10','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_date ADD COLUMN col5 date;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_date SELECT '2013-12-31','2013-12-31','c','2013-12-31';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_date SET col1 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-02-10';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_date SET col2 = '2014-01-01' WHERE col2 = '2014-02-10' AND col1 = '2014-01-01';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_date WHERE col2 = '2014-01-01';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_decimal;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_decimal VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_decimal DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_decimal SELECT 35.00,35.00,'b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_decimal ADD COLUMN col5 decimal;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_decimal SELECT 2.00,2.00,'c',2.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_decimal SET col1 = 1.00 WHERE col2 = 35.00 AND col1 = 35.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_decimal SET col2 = 1.00 WHERE col2 = 35.00 AND col1 = 1.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_decimal WHERE col2 = 1.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_float;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_float VALUES(2.00,2.00,'a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_float DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_float SELECT 35.00,35.00,'b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_float ADD COLUMN col5 float;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_float SELECT 2.00,2.00,'c',2.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_float SET col1 = 1.00 WHERE col2 = 35.00 AND col1 = 35.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_float SET col2 = 1.00 WHERE col2 = 35.00 AND col1 = 1.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_float WHERE col2 = 1.00;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_int;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int VALUES(20000,20000,'a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_int DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int SELECT 35000,35000,'b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_int ADD COLUMN col5 int;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int SELECT 20000,20000,'c',20000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_int SET col1 = 10000 WHERE col2 = 35000 AND col1 = 35000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_int SET col2 = 10000 WHERE col2 = 35000 AND col1 = 10000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_int WHERE col2 = 10000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_int4;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int4 VALUES(20000000,20000000,'a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_int4 DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int4 SELECT 35000000,35000000,'b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_int4 ADD COLUMN col5 int4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int4 SELECT 20000000,20000000,'c',20000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_int4 SET col1 = 10000000 WHERE col2 = 35000000 AND col1 = 35000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_int4 SET col2 = 10000000 WHERE col2 = 35000000 AND col1 = 10000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_int4 WHERE col2 = 10000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_int8;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int8 VALUES(2000000000000000000,2000000000000000000,'a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_int8 DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int8 SELECT 3500000000000000000,3500000000000000000,'b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_int8 ADD COLUMN col5 int8;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_int8 SELECT 2000000000000000000,2000000000000000000,'c',2000000000000000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_int8 SET col1 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col1 = 3500000000000000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_int8 SET col2 = 1000000000000000000 WHERE col2 = 3500000000000000000 AND col1 = 1000000000000000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_int8 WHERE col2 = 1000000000000000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_interval;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_interval VALUES('1 hour','1 hour','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_interval DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_interval SELECT '14 hours','14 hours','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_interval ADD COLUMN col5 interval;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_interval SELECT '1 hour','1 hour','c','1 hour';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_interval SET col1 = '12 hours' WHERE col2 = '14 hours' AND col1 = '14 hours';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_interval SET col2 = '12 hours' WHERE col2 = '14 hours' AND col1 = '12 hours';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_interval WHERE col2 = '12 hours';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_numeric;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_numeric VALUES(2.000000,2.000000,'a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_numeric DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_numeric SELECT 35.000000,35.000000,'b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_numeric ADD COLUMN col5 numeric;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_numeric SELECT 2.000000,2.000000,'c',2.000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_numeric SET col1 = 1.000000 WHERE col2 = 35.000000 AND col1 = 35.000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_numeric SET col2 = 1.000000 WHERE col2 = 35.000000 AND col1 = 1.000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_numeric WHERE col2 = 1.000000;
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_text;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_text
(
    col1 text,
    col2 text,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_text VALUES('abcdefghijklmnop','abcdefghijklmnop','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_text DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_text SELECT 'xyz','xyz','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_text ADD COLUMN col5 text;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_text SELECT 'abcdefghijklmnop','abcdefghijklmnop','c','abcdefghijklmnop';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_text SET col1 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'xyz';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_text SET col2 = 'qrstuvwxyz' WHERE col2 = 'xyz' AND col1 = 'qrstuvwxyz';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_text WHERE col2 = 'qrstuvwxyz';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_text ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_time;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_time VALUES('12:00:00','12:00:00','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_time DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_time SELECT '11:30:00','11:30:00','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_time ADD COLUMN col5 time;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_time SELECT '12:00:00','12:00:00','c','12:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_time SET col1 = '1:00:00' WHERE col2 = '11:30:00' AND col1 = '11:30:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_time SET col2 = '1:00:00' WHERE col2 = '11:30:00' AND col1 = '1:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_time WHERE col2 = '1:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_timestamp;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_timestamp VALUES('2013-12-31 12:00:00','2013-12-31 12:00:00','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_timestamp DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_timestamp SELECT '2014-02-10 12:00:00','2014-02-10 12:00:00','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_timestamp ADD COLUMN col5 timestamp;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_timestamp SELECT '2013-12-31 12:00:00','2013-12-31 12:00:00','c','2013-12-31 12:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_timestamp SET col1 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-02-10 12:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_timestamp SET col2 = '2014-01-01 12:00:00' WHERE col2 = '2014-02-10 12:00:00' AND col1 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_timestamp WHERE col2 = '2014-01-01 12:00:00';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_defpt_dropcol_addcol_dml_timestamptz;
CREATE TABLE mpp21090_defpt_dropcol_addcol_dml_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def
);

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_timestamptz VALUES('2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','a',0);

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_timestamptz DROP COLUMN col4;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_timestamptz SELECT '2014-02-10 12:00:00 PST','2014-02-10 12:00:00 PST','b';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

ALTER TABLE mpp21090_defpt_dropcol_addcol_dml_timestamptz ADD COLUMN col5 timestamptz;

INSERT INTO mpp21090_defpt_dropcol_addcol_dml_timestamptz SELECT '2013-12-31 12:00:00 PST','2013-12-31 12:00:00 PST','c','2013-12-31 12:00:00 PST';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

UPDATE mpp21090_defpt_dropcol_addcol_dml_timestamptz SET col1 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-02-10 12:00:00 PST';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

-- Update partition key
UPDATE mpp21090_defpt_dropcol_addcol_dml_timestamptz SET col2 = '2014-01-01 12:00:00 PST' WHERE col2 = '2014-02-10 12:00:00 PST' AND col1 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM mpp21090_defpt_dropcol_addcol_dml_timestamptz WHERE col2 = '2014-01-01 12:00:00 PST';
SELECT * FROM mpp21090_defpt_dropcol_addcol_dml_timestamptz ORDER BY 1,2,3;

-- @author prabhd
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS dropped_col_tab;
CREATE TABLE dropped_col_tab(
col1 int,
col2 decimal,
col3 char,
col4 date,
col5 int
) distributed by (col1);
INSERT INTO dropped_col_tab VALUES(0,0.00,'a','2014-01-01',0);
SELECT * FROM dropped_col_tab ORDER BY 1,2,3;

ALTER TABLE dropped_col_tab DROP COLUMN col1;
ALTER TABLE dropped_col_tab DROP COLUMN col2;
ALTER TABLE dropped_col_tab DROP COLUMN col3;
ALTER TABLE dropped_col_tab DROP COLUMN col4;
ALTER TABLE dropped_col_tab DROP COLUMN col5;

INSERT INTO dropped_col_tab SELECT 1;
SELECT * FROM dropped_col_tab;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 boolean,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',True,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=False WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 char,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','z',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='-' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 date,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','2013-12-31',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='2014-01-01' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 decimal,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',2.00,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=1.00 WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 float,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',2.00,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=1.00 WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 int4,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',2000000000,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=1000000000 WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 int8,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',2000000000000000000,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=1000000000000000000 WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 int,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',20000,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=10000 WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 interval,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','1 day',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='1 hour' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 numeric,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a',2.000000,1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4=1.000000 WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 text,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','abcdefghijklmnop',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='qrstuvwxyz' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 time,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','12:00:00',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='1:00:00' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 timestamp,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','2013-12-31 12:00:00',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='2014-01-01 12:00:00' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS oidtab;
CREATE TABLE oidtab(
col1 serial,
col2 decimal,
col3 char,
col4 timestamptz,
col5 int
) WITH OIDS DISTRIBUTED by(col4);

ALTER TABLE oidtab DROP COLUMN col2;

INSERT INTO oidtab(col3,col4,col5) SELECT 'a','2013-12-31 12:00:00 PST',1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

DROP TABLE IF EXISTS tempoid;
CREATE TABLE tempoid as SELECT oid,col1,col3,col4,col5 FROM oidtab ORDER BY 1;

UPDATE oidtab SET col4='2014-01-01 12:00:00 PST' WHERE col3 = 'a' AND col5 = 1;
SELECT * FROM oidtab ORDER BY 1,2,3,4;

SELECT * FROM ((SELECT COUNT(*) FROM oidtab) UNION (SELECT COUNT(*) FROM tempoid, oidtab WHERE tempoid.oid = oidtab.oid AND tempoid.col5 = oidtab.col5))foo;










-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_boolean;
CREATE TABLE mpp21090_drop_distcol_dml_boolean(
col1 boolean,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_boolean VALUES(True,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_boolean DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_boolean SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_boolean SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_boolean WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_boolean;
CREATE TABLE mpp21090_drop_distcol_dml_boolean(
col1 boolean,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_boolean VALUES(True,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_boolean DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_boolean SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_boolean SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_boolean WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_boolean;
CREATE TABLE mpp21090_drop_distcol_dml_boolean(
col1 boolean,
col2 decimal,
col3 char,
col4 date,
col5 int
) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_boolean VALUES(True,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_boolean DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_boolean SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_boolean SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_boolean WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_boolean ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_char;
CREATE TABLE mpp21090_drop_distcol_dml_char(
col1 char,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_char VALUES('z',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_char DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_char SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_char SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_char WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_char;
CREATE TABLE mpp21090_drop_distcol_dml_char(
col1 char,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_char VALUES('z',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_char DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_char SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_char SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_char WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_char;
CREATE TABLE mpp21090_drop_distcol_dml_char(
col1 char,
col2 decimal,
col3 char,
col4 date,
col5 int
) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_char VALUES('z',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_char DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_char SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_char SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_char WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_char ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_date;
CREATE TABLE mpp21090_drop_distcol_dml_date(
col1 date,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_date VALUES('2013-12-31',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_date DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_date SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_date SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_date WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_date;
CREATE TABLE mpp21090_drop_distcol_dml_date(
col1 date,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true, orientation= column) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_date VALUES('2013-12-31',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_date DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_date SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_date SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_date WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_date;
CREATE TABLE mpp21090_drop_distcol_dml_date(
col1 date,
col2 decimal,
col3 char,
col4 date,
col5 int
) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_date VALUES('2013-12-31',0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_date DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_date SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_date SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_date WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_date ORDER BY 1,2,3,4;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @optimizer_mode on	
-- @description Tests for MPP-21090
\echo --start_ignore

\echo --end_ignore
DROP TABLE IF EXISTS mpp21090_drop_distcol_dml_decimal;
CREATE TABLE mpp21090_drop_distcol_dml_decimal(
col1 decimal,
col2 decimal,
col3 char,
col4 date,
col5 int
)  with (appendonly= true) distributed by (col1);
INSERT INTO mpp21090_drop_distcol_dml_decimal VALUES(2.00,0.00,'a','2014-01-01',0);
SELECT * FROM mpp21090_drop_distcol_dml_decimal ORDER BY 1,2,3,4;
ALTER TABLE mpp21090_drop_distcol_dml_decimal DROP COLUMN col1;
INSERT INTO mpp21090_drop_distcol_dml_decimal SELECT 1.00,'b','2014-01-02',1;
SELECT * FROM mpp21090_drop_distcol_dml_decimal ORDER BY 1,2,3;
UPDATE mpp21090_drop_distcol_dml_decimal SET col3='c' WHERE col3 = 'b' AND col5 = 1;
SELECT * FROM mpp21090_drop_distcol_dml_decimal ORDER BY 1,2,3;
DELETE FROM mpp21090_drop_distcol_dml_decimal WHERE col3='c';
SELECT * FROM mpp21090_drop_distcol_dml_decimal ORDER BY 1,2,3,4;

-- start_ignore
drop schema sql_partition_1_50 cascade;
-- end_ignore
