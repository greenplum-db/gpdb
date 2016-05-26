
-- start_ignore
create schema dml_functional_no_setup_1;
set search_path to dml_functional_no_setup_1;
-- end_ignore
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_char;
CREATE TABLE changedistpolicy_dml_pttab_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY LIST(col2)(partition partone VALUES('a','b','c','d','e','f','g','h') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo VALUES('i','j','k','l','m','n','o','p') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree VALUES('q','r','s','t','u','v','w','x'));

INSERT INTO changedistpolicy_dml_pttab_char VALUES('g','g','a','g',0);
SELECT * FROM changedistpolicy_dml_pttab_char ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_char SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_char SELECT 'a', 'a','b', 'a', 1;
SELECT * FROM changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_char SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_char WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_date;
CREATE TABLE changedistpolicy_dml_pttab_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01') end('2013-12-31') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31') end('2014-01-01') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01') end('2014-02-01'));

INSERT INTO changedistpolicy_dml_pttab_date VALUES('2013-12-30','2013-12-30','a','2013-12-30',0);
SELECT * FROM changedistpolicy_dml_pttab_date ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_date SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_date SELECT '2014-01-01', '2014-01-01','b', '2014-01-01', 1;
SELECT * FROM changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_date SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_date WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_decimal;
CREATE TABLE changedistpolicy_dml_pttab_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO changedistpolicy_dml_pttab_decimal VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_decimal SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_decimal SELECT 1.00, 1.00,'b', 1.00, 1;
SELECT * FROM changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_decimal SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_decimal WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_float;
CREATE TABLE changedistpolicy_dml_pttab_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00));

INSERT INTO changedistpolicy_dml_pttab_float VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM changedistpolicy_dml_pttab_float ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_float SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_float SELECT 1.00, 1.00,'b', 1.00, 1;
SELECT * FROM changedistpolicy_dml_pttab_float ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_float SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_float ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_float WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_int;
CREATE TABLE changedistpolicy_dml_pttab_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001));

INSERT INTO changedistpolicy_dml_pttab_int VALUES(10000,10000,'a',10000,0);
SELECT * FROM changedistpolicy_dml_pttab_int ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_int SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_int SELECT 20000, 20000,'b', 20000, 1;
SELECT * FROM changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_int SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_int WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_int4;
CREATE TABLE changedistpolicy_dml_pttab_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001));

INSERT INTO changedistpolicy_dml_pttab_int4 VALUES(20000000,20000000,'a',20000000,0);
SELECT * FROM changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_int4 SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_int4 SELECT 10000000, 10000000,'b', 10000000, 1;
SELECT * FROM changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_int4 SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_int4 WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_int8;
CREATE TABLE changedistpolicy_dml_pttab_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001));

INSERT INTO changedistpolicy_dml_pttab_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);
SELECT * FROM changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_int8 SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_int8 SELECT 1000000000000000000, 1000000000000000000,'b', 1000000000000000000, 1;
SELECT * FROM changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_int8 SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_int8 WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_interval;
CREATE TABLE changedistpolicy_dml_pttab_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'));

INSERT INTO changedistpolicy_dml_pttab_interval VALUES('10 secs','10 secs','a','10 secs',0);
SELECT * FROM changedistpolicy_dml_pttab_interval ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_interval SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_interval SELECT '11 hours', '11 hours','b', '11 hours', 1;
SELECT * FROM changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_interval SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_interval WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_numeric;
CREATE TABLE changedistpolicy_dml_pttab_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000));

INSERT INTO changedistpolicy_dml_pttab_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_numeric SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_numeric SELECT 1.000000, 1.000000,'b', 1.000000, 1;
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_numeric SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_numeric WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_time;
CREATE TABLE changedistpolicy_dml_pttab_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('12:00:00') end('18:00:00')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('18:00:00') end('24:00:00') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('0:00:00') end('11:00:00'));

INSERT INTO changedistpolicy_dml_pttab_time VALUES('12:00:00','12:00:00','a','12:00:00',0);
SELECT * FROM changedistpolicy_dml_pttab_time ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_time SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_time SELECT '1:00:00', '1:00:00','b', '1:00:00', 1;
SELECT * FROM changedistpolicy_dml_pttab_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_time SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_time ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_time WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_timestamp;
CREATE TABLE changedistpolicy_dml_pttab_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00') end('2013-12-31 12:00:00') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00') end('2014-01-01 12:00:00') inclusive WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-02 12:00:00') end('2014-02-01 12:00:00'));

INSERT INTO changedistpolicy_dml_pttab_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a','2013-12-30 12:00:00',0);
SELECT * FROM changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_timestamp SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_timestamp SELECT '2014-01-01 12:00:00', '2014-01-01 12:00:00','b', '2014-01-01 12:00:00', 1;
SELECT * FROM changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_timestamp SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_timestamp WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_pttab_timestamptz;
CREATE TABLE changedistpolicy_dml_pttab_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) DISTRIBUTED BY (col1) PARTITION BY RANGE(col2)(partition partone start('2013-12-01 12:00:00 PST') end('2013-12-31 12:00:00 PST') WITH(APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('2013-12-31 12:00:00 PST') end('2014-01-01 12:00:00 PST') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('2014-01-01 12:00:00 PST') end('2014-02-01 12:00:00 PST'));

INSERT INTO changedistpolicy_dml_pttab_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a','2013-12-30 12:00:00 PST',0);
SELECT * FROM changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_pttab_timestamptz SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_pttab_timestamptz SELECT '2014-01-01 12:00:00 PST', '2014-01-01 12:00:00 PST','b', '2014-01-01 12:00:00 PST', 1;
SELECT * FROM changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_pttab_timestamptz SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_pttab_timestamptz WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_pttab_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_char;
CREATE TABLE changedistpolicy_dml_regtab_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_char VALUES('g','g','a','g',0);
SELECT * FROM changedistpolicy_dml_regtab_char ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_char SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_char SELECT 'a', 'a','b', 'a', 1;
SELECT * FROM changedistpolicy_dml_regtab_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_char SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_char ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_char WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_date;
CREATE TABLE changedistpolicy_dml_regtab_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_date VALUES('2013-12-30','2013-12-30','a','2013-12-30',0);
SELECT * FROM changedistpolicy_dml_regtab_date ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_date SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_date SELECT '2014-01-01', '2014-01-01','b', '2014-01-01', 1;
SELECT * FROM changedistpolicy_dml_regtab_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_date SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_date ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_date WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_decimal;
CREATE TABLE changedistpolicy_dml_regtab_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_decimal VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM changedistpolicy_dml_regtab_decimal ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_decimal SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_decimal SELECT 1.00, 1.00,'b', 1.00, 1;
SELECT * FROM changedistpolicy_dml_regtab_decimal ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_decimal SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_decimal ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_decimal WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_float;
CREATE TABLE changedistpolicy_dml_regtab_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_float VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM changedistpolicy_dml_regtab_float ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_float SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_float SELECT 1.00, 1.00,'b', 1.00, 1;
SELECT * FROM changedistpolicy_dml_regtab_float ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_float SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_float ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_float WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_int;
CREATE TABLE changedistpolicy_dml_regtab_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_int VALUES(10000,10000,'a',10000,0);
SELECT * FROM changedistpolicy_dml_regtab_int ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_int SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_int SELECT 20000, 20000,'b', 20000, 1;
SELECT * FROM changedistpolicy_dml_regtab_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_int SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_int ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_int WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_int4;
CREATE TABLE changedistpolicy_dml_regtab_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_int4 VALUES(20000000,20000000,'a',20000000,0);
SELECT * FROM changedistpolicy_dml_regtab_int4 ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_int4 SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_int4 SELECT 10000000, 10000000,'b', 10000000, 1;
SELECT * FROM changedistpolicy_dml_regtab_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_int4 SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_int4 ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_int4 WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_int8;
CREATE TABLE changedistpolicy_dml_regtab_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);
SELECT * FROM changedistpolicy_dml_regtab_int8 ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_int8 SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_int8 SELECT 1000000000000000000, 1000000000000000000,'b', 1000000000000000000, 1;
SELECT * FROM changedistpolicy_dml_regtab_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_int8 SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_int8 ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_int8 WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_interval;
CREATE TABLE changedistpolicy_dml_regtab_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_interval VALUES('10 secs','10 secs','a','10 secs',0);
SELECT * FROM changedistpolicy_dml_regtab_interval ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_interval SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_interval SELECT '12 hours', '12 hours','b', '12 hours', 1;
SELECT * FROM changedistpolicy_dml_regtab_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_interval SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_interval ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_interval WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_numeric;
CREATE TABLE changedistpolicy_dml_regtab_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM changedistpolicy_dml_regtab_numeric ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_numeric SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_numeric SELECT 1.000000, 1.000000,'b', 1.000000, 1;
SELECT * FROM changedistpolicy_dml_regtab_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_numeric SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_numeric ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_numeric WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_char;
CREATE TABLE changedistpolicy_dml_regtab_random_char
(
    col1 char,
    col2 char,
    col3 char,
    col4 char,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_char VALUES('g','g','a','g',0);
SELECT * FROM changedistpolicy_dml_regtab_random_char ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_char SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_char SELECT 'a', 'a','b', 'a', 1;
SELECT * FROM changedistpolicy_dml_regtab_random_char ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_char SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_char ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_char WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_date;
CREATE TABLE changedistpolicy_dml_regtab_random_date
(
    col1 date,
    col2 date,
    col3 char,
    col4 date,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_date VALUES('2013-12-30','2013-12-30','a','2013-12-30',0);
SELECT * FROM changedistpolicy_dml_regtab_random_date ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_date SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_date SELECT '2014-01-01', '2014-01-01','b', '2014-01-01', 1;
SELECT * FROM changedistpolicy_dml_regtab_random_date ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_date SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_date ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_date WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_decimal;
CREATE TABLE changedistpolicy_dml_regtab_random_decimal
(
    col1 decimal,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_decimal VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM changedistpolicy_dml_regtab_random_decimal ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_decimal SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_decimal SELECT 1.00, 1.00,'b', 1.00, 1;
SELECT * FROM changedistpolicy_dml_regtab_random_decimal ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_decimal SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_decimal ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_decimal WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_float;
CREATE TABLE changedistpolicy_dml_regtab_random_float
(
    col1 float,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_float VALUES(2.00,2.00,'a',2.00,0);
SELECT * FROM changedistpolicy_dml_regtab_random_float ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_float SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_float SELECT 1.00, 1.00,'b', 1.00, 1;
SELECT * FROM changedistpolicy_dml_regtab_random_float ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_float SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_float ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_float WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_int;
CREATE TABLE changedistpolicy_dml_regtab_random_int
(
    col1 int,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_int VALUES(10000,10000,'a',10000,0);
SELECT * FROM changedistpolicy_dml_regtab_random_int ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_int SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_int SELECT 20000, 20000,'b', 20000, 1;
SELECT * FROM changedistpolicy_dml_regtab_random_int ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_int SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_int ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_int WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_int4;
CREATE TABLE changedistpolicy_dml_regtab_random_int4
(
    col1 int4,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_int4 VALUES(20000000,20000000,'a',20000000,0);
SELECT * FROM changedistpolicy_dml_regtab_random_int4 ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_int4 SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_int4 SELECT 10000000, 10000000,'b', 10000000, 1;
SELECT * FROM changedistpolicy_dml_regtab_random_int4 ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_int4 SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_int4 ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_int4 WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_int8;
CREATE TABLE changedistpolicy_dml_regtab_random_int8
(
    col1 int8,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_int8 VALUES(200000000000000000,200000000000000000,'a',200000000000000000,0);
SELECT * FROM changedistpolicy_dml_regtab_random_int8 ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_int8 SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_int8 SELECT 1000000000000000000, 1000000000000000000,'b', 1000000000000000000, 1;
SELECT * FROM changedistpolicy_dml_regtab_random_int8 ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_int8 SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_int8 ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_int8 WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_interval;
CREATE TABLE changedistpolicy_dml_regtab_random_interval
(
    col1 interval,
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_interval VALUES('10 secs','10 secs','a','10 secs',0);
SELECT * FROM changedistpolicy_dml_regtab_random_interval ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_interval SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_interval SELECT '11 hours', '11 hours','b', '11 hours', 1;
SELECT * FROM changedistpolicy_dml_regtab_random_interval ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_interval SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_interval ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_interval WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_numeric;
CREATE TABLE changedistpolicy_dml_regtab_random_numeric
(
    col1 numeric,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_numeric VALUES(2.000000,2.000000,'a',2.000000,0);
SELECT * FROM changedistpolicy_dml_regtab_random_numeric ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_numeric SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_numeric SELECT 1.000000, 1.000000,'b', 1.000000, 1;
SELECT * FROM changedistpolicy_dml_regtab_random_numeric ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_numeric SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_numeric ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_numeric WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_time;
CREATE TABLE changedistpolicy_dml_regtab_random_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_time VALUES('12:00:00','12:00:00','a','12:00:00',0);
SELECT * FROM changedistpolicy_dml_regtab_random_time ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_time SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_time SELECT '1:00:00', '1:00:00','b', '1:00:00', 1;
SELECT * FROM changedistpolicy_dml_regtab_random_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_time SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_time ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_time WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_timestamp;
CREATE TABLE changedistpolicy_dml_regtab_random_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a','2013-12-30 12:00:00',0);
SELECT * FROM changedistpolicy_dml_regtab_random_timestamp ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_timestamp SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_timestamp SELECT '2014-01-01 12:00:00', '2014-01-01 12:00:00','b', '2014-01-01 12:00:00', 1;
SELECT * FROM changedistpolicy_dml_regtab_random_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_timestamp SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_timestamp ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_timestamp WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_random_timestamptz;
CREATE TABLE changedistpolicy_dml_regtab_random_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_random_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a','2013-12-30 12:00:00 PST',0);
SELECT * FROM changedistpolicy_dml_regtab_random_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_random_timestamptz SET DISTRIBUTED randomly;

INSERT INTO changedistpolicy_dml_regtab_random_timestamptz SELECT '2014-01-01 12:00:00 PST', '2014-01-01 12:00:00 PST','b', '2014-01-01 12:00:00 PST', 1;
SELECT * FROM changedistpolicy_dml_regtab_random_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_random_timestamptz SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_random_timestamptz ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_random_timestamptz WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_random_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_time;
CREATE TABLE changedistpolicy_dml_regtab_time
(
    col1 time,
    col2 time,
    col3 char,
    col4 time,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_time VALUES('12:00:00','12:00:00','a','12:00:00',0);
SELECT * FROM changedistpolicy_dml_regtab_time ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_time SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_time SELECT '1:00:00', '1:00:00','b', '1:00:00', 1;
SELECT * FROM changedistpolicy_dml_regtab_time ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_time SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_time ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_time WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_timestamp;
CREATE TABLE changedistpolicy_dml_regtab_timestamp
(
    col1 timestamp,
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_timestamp VALUES('2013-12-30 12:00:00','2013-12-30 12:00:00','a','2013-12-30 12:00:00',0);
SELECT * FROM changedistpolicy_dml_regtab_timestamp ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_timestamp SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_timestamp SELECT '2014-01-01 12:00:00', '2014-01-01 12:00:00','b', '2014-01-01 12:00:00', 1;
SELECT * FROM changedistpolicy_dml_regtab_timestamp ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_timestamp SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_timestamp ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_timestamp WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS changedistpolicy_dml_regtab_timestamptz;
CREATE TABLE changedistpolicy_dml_regtab_timestamptz
(
    col1 timestamptz,
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 int
) DISTRIBUTED by (col1);

INSERT INTO changedistpolicy_dml_regtab_timestamptz VALUES('2013-12-30 12:00:00 PST','2013-12-30 12:00:00 PST','a','2013-12-30 12:00:00 PST',0);
SELECT * FROM changedistpolicy_dml_regtab_timestamptz ORDER BY 1,2,3,4;

ALTER TABLE changedistpolicy_dml_regtab_timestamptz SET DISTRIBUTED BY (col3);

INSERT INTO changedistpolicy_dml_regtab_timestamptz SELECT '2014-01-01 12:00:00 PST', '2014-01-01 12:00:00 PST','b', '2014-01-01 12:00:00 PST', 1;
SELECT * FROM changedistpolicy_dml_regtab_timestamptz ORDER BY 1,2,3;

-- Update distribution key
UPDATE changedistpolicy_dml_regtab_timestamptz SET col3 ='c' WHERE col3 ='b' AND col5 = 1;
SELECT * FROM changedistpolicy_dml_regtab_timestamptz ORDER BY 1,2,3;

DELETE FROM changedistpolicy_dml_regtab_timestamptz WHERE col3 ='c';
SELECT * FROM changedistpolicy_dml_regtab_timestamptz ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_char;
CREATE TABLE colalias_dml_char
(
    col1 char DEFAULT 'a',
    col2 char,
    col3 char,
    col4 char,
    col5 char
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_char_candidate;
CREATE TABLE colalias_dml_char_candidate
(
    col1 char DEFAULT 'a',
    col2 char,
    col3 char,
    col4 char,
    col5 char
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_char_candidate VALUES('g','a','a','g','a');

INSERT INTO colalias_dml_char(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_char_candidate)foo;
SELECT * FROM colalias_dml_char ORDER BY 1,2,3,4;

UPDATE colalias_dml_char SET col1 = (select col2 as col1 FROM colalias_dml_char_candidate);
SELECT * FROM colalias_dml_char ORDER BY 1,2,3,4;

UPDATE colalias_dml_char SET col1 =colalias_dml_char_candidate.col2 FROM colalias_dml_char_candidate;
SELECT * FROM colalias_dml_char ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_date;
CREATE TABLE colalias_dml_date
(
    col1 date DEFAULT '2014-01-01',
    col2 date,
    col3 char,
    col4 date,
    col5 date
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_date_candidate;
CREATE TABLE colalias_dml_date_candidate
(
    col1 date DEFAULT '2014-01-01',
    col2 date,
    col3 char,
    col4 date,
    col5 date
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_date_candidate VALUES('2013-12-30','2014-01-01','a','2013-12-30','2014-01-01');

INSERT INTO colalias_dml_date(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_date_candidate)foo;
SELECT * FROM colalias_dml_date ORDER BY 1,2,3,4;

UPDATE colalias_dml_date SET col1 = (select col2 as col1 FROM colalias_dml_date_candidate);
SELECT * FROM colalias_dml_date ORDER BY 1,2,3,4;

UPDATE colalias_dml_date SET col1 =colalias_dml_date_candidate.col2 FROM colalias_dml_date_candidate;
SELECT * FROM colalias_dml_date ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_decimal;
CREATE TABLE colalias_dml_decimal
(
    col1 decimal DEFAULT 1.00,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 decimal
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_decimal_candidate;
CREATE TABLE colalias_dml_decimal_candidate
(
    col1 decimal DEFAULT 1.00,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 decimal
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_decimal_candidate VALUES(2.00,1.00,'a',2.00,1.00);

INSERT INTO colalias_dml_decimal(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_decimal_candidate)foo;
SELECT * FROM colalias_dml_decimal ORDER BY 1,2,3,4;

UPDATE colalias_dml_decimal SET col1 = (select col2 as col1 FROM colalias_dml_decimal_candidate);
SELECT * FROM colalias_dml_decimal ORDER BY 1,2,3,4;

UPDATE colalias_dml_decimal SET col1 =colalias_dml_decimal_candidate.col2 FROM colalias_dml_decimal_candidate;
SELECT * FROM colalias_dml_decimal ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_float;
CREATE TABLE colalias_dml_float
(
    col1 float DEFAULT 1.00,
    col2 float,
    col3 char,
    col4 float,
    col5 float
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_float_candidate;
CREATE TABLE colalias_dml_float_candidate
(
    col1 float DEFAULT 1.00,
    col2 float,
    col3 char,
    col4 float,
    col5 float
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_float_candidate VALUES(2.00,1.00,'a',2.00,1.00);

INSERT INTO colalias_dml_float(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_float_candidate)foo;
SELECT * FROM colalias_dml_float ORDER BY 1,2,3,4;

UPDATE colalias_dml_float SET col1 = (select col2 as col1 FROM colalias_dml_float_candidate);
SELECT * FROM colalias_dml_float ORDER BY 1,2,3,4;

UPDATE colalias_dml_float SET col1 =colalias_dml_float_candidate.col2 FROM colalias_dml_float_candidate;
SELECT * FROM colalias_dml_float ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_int;
CREATE TABLE colalias_dml_int
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_int_candidate;
CREATE TABLE colalias_dml_int_candidate
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_int_candidate VALUES(10000,20000,'a',10000,20000);

INSERT INTO colalias_dml_int(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_int_candidate)foo;
SELECT * FROM colalias_dml_int ORDER BY 1,2,3,4;

UPDATE colalias_dml_int SET col1 = (select col2 as col1 FROM colalias_dml_int_candidate);
SELECT * FROM colalias_dml_int ORDER BY 1,2,3,4;

UPDATE colalias_dml_int SET col1 =colalias_dml_int_candidate.col2 FROM colalias_dml_int_candidate;
SELECT * FROM colalias_dml_int ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_int4;
CREATE TABLE colalias_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int4
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_int4_candidate;
CREATE TABLE colalias_dml_int4_candidate
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int4
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_int4_candidate VALUES(20000000,10000000,'a',20000000,10000000);

INSERT INTO colalias_dml_int4(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_int4_candidate)foo;
SELECT * FROM colalias_dml_int4 ORDER BY 1,2,3,4;

UPDATE colalias_dml_int4 SET col1 = (select col2 as col1 FROM colalias_dml_int4_candidate);
SELECT * FROM colalias_dml_int4 ORDER BY 1,2,3,4;

UPDATE colalias_dml_int4 SET col1 =colalias_dml_int4_candidate.col2 FROM colalias_dml_int4_candidate;
SELECT * FROM colalias_dml_int4 ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_int8;
CREATE TABLE colalias_dml_int8
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int8
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_int8_candidate;
CREATE TABLE colalias_dml_int8_candidate
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int8
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_int8_candidate VALUES(200000000000000000,1000000000000000000,'a',200000000000000000,1000000000000000000);

INSERT INTO colalias_dml_int8(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_int8_candidate)foo;
SELECT * FROM colalias_dml_int8 ORDER BY 1,2,3,4;

UPDATE colalias_dml_int8 SET col1 = (select col2 as col1 FROM colalias_dml_int8_candidate);
SELECT * FROM colalias_dml_int8 ORDER BY 1,2,3,4;

UPDATE colalias_dml_int8 SET col1 =colalias_dml_int8_candidate.col2 FROM colalias_dml_int8_candidate;
SELECT * FROM colalias_dml_int8 ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_interval;
CREATE TABLE colalias_dml_interval
(
    col1 interval DEFAULT '11 hours',
    col2 interval,
    col3 char,
    col4 interval,
    col5 interval
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_interval_candidate;
CREATE TABLE colalias_dml_interval_candidate
(
    col1 interval DEFAULT '11 hours',
    col2 interval,
    col3 char,
    col4 interval,
    col5 interval
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_interval_candidate VALUES('10 secs','11 hours','a','10 secs','11 hours');

INSERT INTO colalias_dml_interval(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_interval_candidate)foo;
SELECT * FROM colalias_dml_interval ORDER BY 1,2,3,4;

UPDATE colalias_dml_interval SET col1 = (select col2 as col1 FROM colalias_dml_interval_candidate);
SELECT * FROM colalias_dml_interval ORDER BY 1,2,3,4;

UPDATE colalias_dml_interval SET col1 =colalias_dml_interval_candidate.col2 FROM colalias_dml_interval_candidate;
SELECT * FROM colalias_dml_interval ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_numeric;
CREATE TABLE colalias_dml_numeric
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 numeric
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_numeric_candidate;
CREATE TABLE colalias_dml_numeric_candidate
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 numeric
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_numeric_candidate VALUES(2.000000,1.000000,'a',2.000000,1.000000);

INSERT INTO colalias_dml_numeric(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_numeric_candidate)foo;
SELECT * FROM colalias_dml_numeric ORDER BY 1,2,3,4;

UPDATE colalias_dml_numeric SET col1 = (select col2 as col1 FROM colalias_dml_numeric_candidate);
SELECT * FROM colalias_dml_numeric ORDER BY 1,2,3,4;

UPDATE colalias_dml_numeric SET col1 =colalias_dml_numeric_candidate.col2 FROM colalias_dml_numeric_candidate;
SELECT * FROM colalias_dml_numeric ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_time;
CREATE TABLE colalias_dml_time
(
    col1 time DEFAULT '1:00:00',
    col2 time,
    col3 char,
    col4 time,
    col5 time
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_time_candidate;
CREATE TABLE colalias_dml_time_candidate
(
    col1 time DEFAULT '1:00:00',
    col2 time,
    col3 char,
    col4 time,
    col5 time
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_time_candidate VALUES('12:00:00','1:00:00','a','12:00:00','1:00:00');

INSERT INTO colalias_dml_time(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_time_candidate)foo;
SELECT * FROM colalias_dml_time ORDER BY 1,2,3,4;

UPDATE colalias_dml_time SET col1 = (select col2 as col1 FROM colalias_dml_time_candidate);
SELECT * FROM colalias_dml_time ORDER BY 1,2,3,4;

UPDATE colalias_dml_time SET col1 =colalias_dml_time_candidate.col2 FROM colalias_dml_time_candidate;
SELECT * FROM colalias_dml_time ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_timestamp;
CREATE TABLE colalias_dml_timestamp
(
    col1 timestamp DEFAULT '2014-01-01 12:00:00',
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 timestamp
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_timestamp_candidate;
CREATE TABLE colalias_dml_timestamp_candidate
(
    col1 timestamp DEFAULT '2014-01-01 12:00:00',
    col2 timestamp,
    col3 char,
    col4 timestamp,
    col5 timestamp
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_timestamp_candidate VALUES('2013-12-30 12:00:00','2014-01-01 12:00:00','a','2013-12-30 12:00:00','2014-01-01 12:00:00');

INSERT INTO colalias_dml_timestamp(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_timestamp_candidate)foo;
SELECT * FROM colalias_dml_timestamp ORDER BY 1,2,3,4;

UPDATE colalias_dml_timestamp SET col1 = (select col2 as col1 FROM colalias_dml_timestamp_candidate);
SELECT * FROM colalias_dml_timestamp ORDER BY 1,2,3,4;

UPDATE colalias_dml_timestamp SET col1 =colalias_dml_timestamp_candidate.col2 FROM colalias_dml_timestamp_candidate;
SELECT * FROM colalias_dml_timestamp ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS colalias_dml_timestamptz;
CREATE TABLE colalias_dml_timestamptz
(
    col1 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 timestamptz
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);

DROP TABLE IF EXISTS colalias_dml_timestamptz_candidate;
CREATE TABLE colalias_dml_timestamptz_candidate
(
    col1 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col2 timestamptz,
    col3 char,
    col4 timestamptz,
    col5 timestamptz
) DISTRIBUTED by (col2);

INSERT INTO colalias_dml_timestamptz_candidate VALUES('2013-12-30 12:00:00 PST','2014-01-01 12:00:00 PST','a','2013-12-30 12:00:00 PST','2014-01-01 12:00:00 PST');

INSERT INTO colalias_dml_timestamptz(col2,col1,col3,col5,col4) SELECT col1,col2,col3,col5,col4 FROM (SELECT col1,col1 as col2,col3,col5 as col4,col5  FROM colalias_dml_timestamptz_candidate)foo;
SELECT * FROM colalias_dml_timestamptz ORDER BY 1,2,3,4;

UPDATE colalias_dml_timestamptz SET col1 = (select col2 as col1 FROM colalias_dml_timestamptz_candidate);
SELECT * FROM colalias_dml_timestamptz ORDER BY 1,2,3,4;

UPDATE colalias_dml_timestamptz SET col1 =colalias_dml_timestamptz_candidate.col2 FROM colalias_dml_timestamptz_candidate;
SELECT * FROM colalias_dml_timestamptz ORDER BY 1,2,3,4;


-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_decimal;
CREATE TABLE computedcol_dml_decimal
(
    col1 decimal DEFAULT 1.00,
    col2 decimal,
    col3 char,
    col4 decimal,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00), default partition def);

INSERT INTO computedcol_dml_decimal(col2,col1,col3,col5,col4) SELECT 2.00::decimal+1.00::decimal, 2.00::decimal+1.00::decimal, 'a', 1,2.00::decimal+1.00::decimal;
INSERT INTO computedcol_dml_decimal(col2,col3,col5,col4) SELECT 2.00::decimal+1.00::decimal,'a', 1,2.00::decimal+1.00::decimal; 
SELECT * FROM computedcol_dml_decimal ORDER BY 1,2,3,4;

UPDATE computedcol_dml_decimal SET col1=2.00::decimal+1.00::decimal;
SELECT * FROM computedcol_dml_decimal ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_decimal WHERE col1=1.00::decimal + 2.00::decimal;
SELECT * FROM computedcol_dml_decimal ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_float;
CREATE TABLE computedcol_dml_float
(
    col1 float DEFAULT 1.00,
    col2 float,
    col3 char,
    col4 float,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.00) end(10.00)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.00) end(20.00) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.00) end(30.00), default partition def);

INSERT INTO computedcol_dml_float(col2,col1,col3,col5,col4) SELECT 2.00::float+1.00::float, 2.00::float+1.00::float, 'a', 1,2.00::float+1.00::float;
INSERT INTO computedcol_dml_float(col2,col3,col5,col4) SELECT 2.00::float+1.00::float,'a', 1,2.00::float+1.00::float; 
SELECT * FROM computedcol_dml_float ORDER BY 1,2,3,4;

UPDATE computedcol_dml_float SET col1=2.00::float+1.00::float;
SELECT * FROM computedcol_dml_float ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_float WHERE col1=1.00::float + 2.00::float;
SELECT * FROM computedcol_dml_float ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_int;
CREATE TABLE computedcol_dml_int
(
    col1 int DEFAULT 20000,
    col2 int,
    col3 char,
    col4 int,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(10001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10001) end(20001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20001) end(30001), default partition def);

INSERT INTO computedcol_dml_int(col2,col1,col3,col5,col4) SELECT 10000::int+20000::int, 10000::int+20000::int, 'a', 1,10000::int+20000::int;
INSERT INTO computedcol_dml_int(col2,col3,col5,col4) SELECT 10000::int+20000::int,'a', 1,10000::int+20000::int; 
SELECT * FROM computedcol_dml_int ORDER BY 1,2,3,4;

UPDATE computedcol_dml_int SET col1=10000::int+20000::int;
SELECT * FROM computedcol_dml_int ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_int WHERE col1=20000::int + 10000::int;
SELECT * FROM computedcol_dml_int ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_int4;
CREATE TABLE computedcol_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4,
    col3 char,
    col4 int4,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(100000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(100000001) end(200000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(200000001) end(300000001), default partition def);

INSERT INTO computedcol_dml_int4(col2,col1,col3,col5,col4) SELECT 20000000::int4+10000000::int4, 20000000::int4+10000000::int4, 'a', 1,20000000::int4+10000000::int4;
INSERT INTO computedcol_dml_int4(col2,col3,col5,col4) SELECT 20000000::int4+10000000::int4,'a', 1,20000000::int4+10000000::int4; 
SELECT * FROM computedcol_dml_int4 ORDER BY 1,2,3,4;

UPDATE computedcol_dml_int4 SET col1=20000000::int4+10000000::int4;
SELECT * FROM computedcol_dml_int4 ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_int4 WHERE col1=10000000::int4 + 20000000::int4;
SELECT * FROM computedcol_dml_int4 ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_int8;
CREATE TABLE computedcol_dml_int8
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8,
    col3 char,
    col4 int8,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1) end(1000000000000000001)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(1000000000000000001) end(2000000000000000001) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(2000000000000000001) end(3000000000000000001), default partition def);

INSERT INTO computedcol_dml_int8(col2,col1,col3,col5,col4) SELECT 200000000000000000::int8+1000000000000000000::int8, 200000000000000000::int8+1000000000000000000::int8, 'a', 1,200000000000000000::int8+1000000000000000000::int8;
INSERT INTO computedcol_dml_int8(col2,col3,col5,col4) SELECT 200000000000000000::int8+1000000000000000000::int8,'a', 1,200000000000000000::int8+1000000000000000000::int8; 
SELECT * FROM computedcol_dml_int8 ORDER BY 1,2,3,4;

UPDATE computedcol_dml_int8 SET col1=200000000000000000::int8+1000000000000000000::int8;
SELECT * FROM computedcol_dml_int8 ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_int8 WHERE col1=1000000000000000000::int8 + 200000000000000000::int8;
SELECT * FROM computedcol_dml_int8 ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_interval;
CREATE TABLE computedcol_dml_interval
(
    col1 interval DEFAULT '11 hours',
    col2 interval,
    col3 char,
    col4 interval,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start('1 sec') end('1 min')  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start('1 min') end('1 hour') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start('1 hour') end('12 hours'), default partition def);

INSERT INTO computedcol_dml_interval(col2,col1,col3,col5,col4) SELECT '10 secs'::interval+'11 hours'::interval, '10 secs'::interval+'11 hours'::interval, 'a', 1,'10 secs'::interval+'11 hours'::interval;
INSERT INTO computedcol_dml_interval(col2,col3,col5,col4) SELECT '10 secs'::interval+'11 hours'::interval,'a', 1,'10 secs'::interval+'11 hours'::interval; 
SELECT * FROM computedcol_dml_interval ORDER BY 1,2,3,4;

UPDATE computedcol_dml_interval SET col1='10 secs'::interval+'11 hours'::interval;
SELECT * FROM computedcol_dml_interval ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_interval WHERE col1='11 hours'::interval + '10 secs'::interval;
SELECT * FROM computedcol_dml_interval ORDER BY 1,2,3,4;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS computedcol_dml_numeric;
CREATE TABLE computedcol_dml_numeric
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric,
    col3 char,
    col4 numeric,
    col5 int
) 
DISTRIBUTED by (col1)
PARTITION BY RANGE(col2)(partition partone start(1.000000) end(10.000000)  WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),partition parttwo start(10.000000) end(20.000000) WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),partition partthree start(20.000000) end(30.000000), default partition def);

INSERT INTO computedcol_dml_numeric(col2,col1,col3,col5,col4) SELECT 2.000000::numeric+1.000000::numeric, 2.000000::numeric+1.000000::numeric, 'a', 1,2.000000::numeric+1.000000::numeric;
INSERT INTO computedcol_dml_numeric(col2,col3,col5,col4) SELECT 2.000000::numeric+1.000000::numeric,'a', 1,2.000000::numeric+1.000000::numeric; 
SELECT * FROM computedcol_dml_numeric ORDER BY 1,2,3,4;

UPDATE computedcol_dml_numeric SET col1=2.000000::numeric+1.000000::numeric;
SELECT * FROM computedcol_dml_numeric ORDER BY 1,2,3,4;

DELETE FROM computedcol_dml_numeric WHERE col1=1.000000::numeric + 2.000000::numeric;
SELECT * FROM computedcol_dml_numeric ORDER BY 1,2,3,4;

-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description Mpp-19671




SET test_print_direct_dispatch_info = on;
SET gp_autostats_mode=none;

--start_ignore
DROP TABLE IF EXISTS T;
--end_ignore
CREATE TABLE T ( a int , b int ) DISTRIBUTED BY (a);
INSERT INTO T VALUES (1,2);
INSERT INTO T VALUES (1,2);
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Insert on AO tables within transaction




DROP TABLE IF EXISTS dml_ao;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_ao;
    CREATE TABLE dml_ao (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
SAVEPOINT sp2;
    INSERT INTO dml_ao values (1,'to be exchanged','a','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
ROLLBACK to sp2;
    INSERT INTO dml_ao values (1,'exchanged','a','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_ao;
SELECT x FROM dml_ao;
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Negative test - Update on AO tables within transaction




DROP TABLE IF EXISTS dml_ao;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_ao;
    CREATE TABLE dml_ao (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_ao values (7,'to be exchanged','s','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    UPDATE dml_ao SET x = 'update_ao';
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_ao;
SELECT x FROM dml_ao;

-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Negative test - Delete on AO tables within transaction




DROP TABLE IF EXISTS dml_ao;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_ao;
    CREATE TABLE dml_ao (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_ao values(generate_series(1,9),'create table with subtransactions','s','subtransaction table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    DELETE FROM dml_ao;
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_ao;
SELECT x FROM dml_ao;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bigint




DROP TABLE IF EXISTS dml_tab_bigint;
CREATE TABLE dml_tab_bigint (a bigint DEFAULT 1234567891011) distributed by (a);

-- Simple DML
INSERT INTO dml_tab_bigint values(-9223372036854775808);
INSERT INTO dml_tab_bigint values(9223372036854775807);
SELECT * FROM dml_tab_bigint ORDER BY 1;
INSERT INTO dml_tab_bigint DEFAULT VALUES;
SELECT * FROM dml_tab_bigint ORDER BY 1;
UPDATE dml_tab_bigint SET a = 9223372036854775807;
SELECT * FROM dml_tab_bigint ORDER BY 1;
UPDATE dml_tab_bigint SET a = -9223372036854775808;
SELECT * FROM dml_tab_bigint ORDER BY 1;

-- OUT OF RANGE
SELECT * FROM dml_tab_bigint ORDER BY 1;
INSERT INTO dml_tab_bigint values(-9223372036854775809);
SELECT * FROM dml_tab_bigint ORDER BY 1;
INSERT INTO dml_tab_bigint values(9223372036854775808);
SELECT * FROM dml_tab_bigint ORDER BY 1;
UPDATE dml_tab_bigint SET a = -9223372036854775809;
SELECT * FROM dml_tab_bigint ORDER BY 1;
UPDATE dml_tab_bigint SET a = 9223372036854775808;
SELECT * FROM dml_tab_bigint ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bigserial




DROP TABLE IF EXISTS dml_bigserial;
CREATE TABLE dml_bigserial (a bigserial) distributed by (a);

-- Simple DML
INSERT INTO dml_bigserial VALUES(9223372036854775807);
SELECT * FROM dml_bigserial ORDER BY 1;
INSERT INTO dml_bigserial VALUES(0);
SELECT * FROM dml_bigserial ORDER BY 1;
UPDATE dml_bigserial SET a = 9223372036854775807;
SELECT * FROM dml_bigserial ORDER BY 1;
UPDATE dml_bigserial SET a = 0;
SELECT * FROM dml_bigserial ORDER BY 1;

-- OUT OF RANGE
INSERT INTO dml_bigserial VALUES(9223372036854775808);
SELECT * FROM dml_bigserial ORDER BY 1;
UPDATE dml_bigserial SET a = 9223372036854775808;
SELECT * FROM dml_bigserial ORDER BY 1;


-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bit




DROP TABLE IF EXISTS dml_bit;
CREATE TABLE dml_bit( a bit ) distributed by (a);

-- Simple DML
INSERT INTO dml_bit VALUES('1');
SELECT * FROM dml_bit ORDER BY 1;
UPDATE dml_bit SET a = '0';
SELECT * FROM dml_bit ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_bit VALUES('True');
SELECT * FROM dml_bit ORDER BY 1;
INSERT INTO dml_bit VALUES('11');
SELECT * FROM dml_bit ORDER BY 1;
UPDATE dml_bit SET a = '00';
SELECT * FROM dml_bit ORDER BY 1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bit varying




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
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bool





DROP TABLE IF EXISTS dml_bool;
CREATE TABLE dml_bool (a bool) distributed by (a);

-- Simple DML
INSERT INTO dml_bool VALUES('True');
SELECT * FROM dml_bool ORDER BY 1;
INSERT INTO dml_bool VALUES('False');
SELECT * FROM dml_bool ORDER BY 1;
UPDATE dml_bool SET a = 'True';
SELECT * FROM dml_bool ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for bytea




DROP TABLE IF EXISTS dml_bytea;
CREATE TABLE dml_bytea( a bytea) distributed by (a);

-- Simple DML
INSERT INTO dml_bytea VALUES(pg_catalog.decode(array_to_string(ARRAY(SELECT chr((65 + round(random() * 25)) :: integer) FROM generate_series(1, 10490000)), ''),'escape'));

SELECT COUNT(*) FROM dml_bytea;

UPDATE dml_bytea SET a = pg_catalog.decode(array_to_string(ARRAY(SELECT chr((65 + round(random() * 25)) :: integer) FROM generate_series(1, 10490000)), ''),'escape'); 


-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for char





DROP TABLE IF EXISTS dml_char;
CREATE TABLE dml_char( a char) distributed by (a);
-- Simple DML
INSERT INTO dml_char VALUES('1');
SELECT * FROM dml_char ORDER BY 1;
UPDATE dml_char SET a = 'b';
SELECT * FROM dml_char ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_char VALUES('ab');
SELECT * FROM dml_char ORDER BY 1;
INSERT INTO dml_char VALUES('-a');
SELECT * FROM dml_char ORDER BY 1;
UPDATE dml_char SET a ='-1';
SELECT * FROM dml_char ORDER BY 1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for cidr




DROP TABLE IF EXISTS dml_cidr;
CREATE TABLE dml_cidr (a cidr) distributed by (a);

-- Simple DML
INSERT INTO dml_cidr VALUES('192.168.100.128/25');
SELECT * FROM dml_cidr ORDER BY 1;
INSERT INTO dml_cidr VALUES('128');
SELECT * FROM dml_cidr ORDER BY 1;
INSERT INTO dml_cidr VALUES('2001:4f8:3:ba:2e0:81ff:fe22:d1f1/128');
SELECT * FROM dml_cidr ORDER BY 1;
UPDATE dml_cidr SET a = '192.168.100.128/25';
SELECT * FROM dml_cidr ORDER BY 1;

-- OUT OF RANGE
INSERT INTO dml_cidr VALUES('204.248.199.199/30');
SELECT * FROM dml_cidr ORDER BY 1;
INSERT INTO dml_cidr VALUES('2001:4f8:3:ba:2e0:81ff:fe22:d1f1/120');
SELECT * FROM dml_cidr ORDER BY 1;
UPDATE dml_cidr SET a = '204.248.199.199/30';
SELECT * FROM dml_cidr ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for date




DROP TABLE IF EXISTS dml_date;
CREATE TABLE dml_date ( a date) distributed by (a);

-- Simple DML
INSERT INTO dml_date VALUES ('2013-01-01 08:00:00.000000'::date);
SELECT * FROM dml_date ORDER BY 1;
INSERT INTO dml_date VALUES ('4713-01-01 BC');
SELECT * FROM dml_date ORDER BY 1;
INSERT INTO dml_date select to_date('3232098', 'MM/DD/YYYY');
SELECT * FROM dml_date ORDER BY 1;
UPDATE dml_date SET a = '4713-01-01 BC';
SELECT * FROM dml_date ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_date VALUES ('0000-01-01 08:00:00.000000'::date);
SELECT * FROM dml_date ORDER BY 1;
INSERT INTO dml_date VALUES ('4714-01-01 BC');
SELECT * FROM dml_date ORDER BY 1;
UPDATE dml_date SET a = '4714-01-01 BC';
SELECT * FROM dml_date ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for double precision





DROP TABLE IF EXISTS dml_dp;
CREATE TABLE dml_dp ( a double precision) distributed by (a);

-- Simple DML
INSERT INTO dml_dp VALUES('5.7107617381473e+45');
SELECT * FROM dml_dp ORDER BY 1;
INSERT INTO dml_dp VALUES('5.7107617381473e-10');
SELECT * FROM dml_dp ORDER BY 1;
UPDATE dml_dp SET a = 5.7107617381473e+45;
SELECT * FROM dml_dp ORDER BY 1;

-- OUT OF RANGE
INSERT INTO dml_dp VALUES('5.7107617381473e+1000');
SELECT * FROM dml_dp ORDER BY 1;
INSERT INTO dml_dp VALUES('5.7107617381473e-1000');
SELECT * FROM dml_dp ORDER BY 1;
UPDATE dml_dp SET a = 5.7107617381473e+1000;
SELECT * FROM dml_dp ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for inet




DROP TABLE IF EXISTS dml_inet;
CREATE TABLE dml_inet(a inet) distributed by (a);

-- Simple DML
INSERT INTO dml_inet VALUES ('192.168.1.6');
SELECT * FROM dml_inet ORDER BY 1;
INSERT INTO dml_inet VALUES ('204.248.199.199/30');
SELECT * FROM dml_inet ORDER BY 1;
INSERT INTO dml_inet VALUES('::1');
SELECT * FROM dml_inet ORDER BY 1;
UPDATE dml_inet SET a = '::1';
SELECT * FROM dml_inet ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_inet VALUES ('192.168.23.20/33');
SELECT * FROM dml_inet ORDER BY 1;
INSERT INTO dml_inet VALUES('');
SELECT * FROM dml_inet ORDER BY 1;
UPDATE dml_inet SET a = '192.168.23.20/33';
SELECT * FROM dml_inet ORDER BY 1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @gpopt 1.473
-- @description test1: Boundary test for integer array




DROP TABLE IF EXISTS dml_intarr;
CREATE TABLE dml_intarr (a integer[] DEFAULT '{5,4,3,2,1}') distributed by (a);

-- Simple DML
INSERT INTO dml_intarr VALUES('{6,7,8,9,10}');
SELECT * FROM dml_intarr ORDER BY 1;
INSERT INTO dml_intarr DEFAULT VALUES;
SELECT * FROM dml_intarr ORDER BY 1;
SELECT a[1] FROM dml_intarr ORDER BY 1;
UPDATE dml_intarr SET a = '{11,12}';
SELECT * FROM dml_intarr ORDER BY 1;
UPDATE dml_intarr SET a[1] = 111;
SELECT a[1] FROM dml_intarr ORDER BY 1;
-- @author prabhd 
-- @CREATEd 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for integer




DROP TABLE IF EXISTS dml_integer;
CREATE TABLE dml_integer (a integer) distributed by (a);

-- Simple DML
INSERT INTO dml_integer VALUES(-2147483648);
SELECT * FROM dml_integer ORDER BY 1;
INSERT INTO dml_integer VALUES(2147483647);
SELECT * FROM dml_integer ORDER BY 1;
UPDATE dml_integer SET a = 2147483647;
SELECT * FROM dml_integer ORDER BY 1;
UPDATE dml_integer SET a = -2147483648;
SELECT * FROM dml_integer ORDER BY 1;

-- OUT OF RANGE VALUES
INSERT INTO dml_integer VALUES(-2147483649);
SELECT * FROM dml_integer ORDER BY 1;
INSERT INTO dml_integer VALUES(2147483648);
SELECT * FROM dml_integer ORDER BY 1;
UPDATE dml_integer SET a = 2147483648;
SELECT * FROM dml_integer ORDER BY 1;
UPDATE dml_integer SET a = -2147483649;
SELECT * FROM dml_integer ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for interval




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


-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for macaddr




DROP TABLE IF EXISTS dml_macaddr;
CREATE TABLE dml_macaddr( a macaddr) distributed by (a);
-- SIMPLE INSERTS
INSERT INTO dml_macaddr VALUES('08002b:010203');
SELECT * FROM dml_macaddr ORDER BY 1;
UPDATE dml_macaddr SET a = 'FF:FF:FF:FF:FF:FF';
SELECT * FROM dml_macaddr ORDER BY 1;

--INVALID VALUES
INSERT INTO dml_macaddr VALUES('FF:FF:FF:FF:FF:FF0');
SELECT * FROM dml_macaddr ORDER BY 1;
UPDATE dml_macaddr SET a = 'FF:FF:FF:FF:FF:FF0';
SELECT * FROM dml_macaddr ORDER BY 1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @skip 'Money datatype was enlarged to 64 bits in PostgreSQL 8.3 so will be tested via installcheck-good' 
-- @db_name dmldb
-- @description test: Boundary test for money




DROP TABLE IF EXISTS dml_money;
CREATE TABLE dml_money (a money) distributed by (a);

-- SIMPLE INSERTS
INSERT INTO dml_money VALUES('-2147483648'::money);
SELECT * FROM dml_money ORDER BY 1;
INSERT INTO dml_money VALUES('2147483647'::money);
SELECT * FROM dml_money ORDER BY 1;
UPDATE dml_money SET a = '2147483647'::money;
SELECT * FROM dml_money ORDER BY 1;
UPDATE dml_money SET a = '-2147483648'::money;
SELECT * FROM dml_money ORDER BY 1;

-- OUT OF RANGE (no error observed)
INSERT INTO dml_money VALUES('-2147483649'::money);
SELECT * FROM dml_money ORDER BY 1;
INSERT INTO dml_money VALUES('2147483648'::money);
SELECT * FROM dml_money ORDER BY 1;
UPDATE dml_money SET a = '2147483648'::money;
SELECT * FROM dml_money ORDER BY 1;
UPDATE dml_money SET a = '-2147483649'::money;
SELECT * FROM dml_money ORDER BY 1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for numeric




DROP TABLE IF EXISTS dml_numeric;
CREATE TABLE dml_numeric( a numeric) distributed by (a);

-- Simple DML
INSERT INTO dml_numeric VALUES (10e+1000);
SELECT * FROM dml_numeric ORDER BY 1;
INSERT INTO dml_numeric VALUES (1e-1000);
SELECT * FROM dml_numeric ORDER BY 1;

-- OUT OF RANGE
INSERT INTO dml_numeric VALUES (1e+10000);
SELECT * FROM dml_numeric ORDER BY 1;
UPDATE dml_numeric SET a = 1e+10000;
SELECT * FROM dml_numeric ORDER BY 1;



-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for numeric




DROP TABLE IF EXISTS dml_numeric2;
CREATE TABLE dml_numeric2(a numeric(5,2)) distributed by (a);

-- Simple DML
INSERT  INTO  dml_numeric2 VALUES (1.00e+2);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+1;
SELECT * FROM dml_numeric2 ORDER BY 1;

--OUT OF RANGE
INSERT  INTO  dml_numeric2 VALUES (1.00e+3);
SELECT * FROM dml_numeric2 ORDER BY 1;
UPDATE dml_numeric2 SET a = 1.00e+3;
SELECT * FROM dml_numeric2 ORDER BY 1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for real




DROP TABLE IF EXISTS dml_real;
CREATE TABLE dml_real ( a real) distributed by (a);

-- Simple DML
INSERT INTO dml_real VALUES(0);
SELECT * FROM dml_real ORDER BY 1;
INSERT INTO dml_real VALUES('-1.18e-38');
SELECT * FROM dml_real ORDER BY 1;
UPDATE dml_real SET a = 0;
SELECT * FROM dml_real ORDER BY 1;


-- OUT OF RANGE VALUES
INSERT INTO dml_real VALUES('-3.40e+39');
SELECT * FROM dml_real ORDER BY 1;
INSERT INTO dml_real VALUES('-1.00e-46');
SELECT * FROM dml_real ORDER BY 1;
UPDATE dml_real SET a = '-1.00e-46';
SELECT * FROM dml_real ORDER BY 1;

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for smallint




DROP TABLE IF EXISTS dml_tab_smallint;
CREATE TABLE dml_tab_smallint (a smallint) distributed by (a);

-- Simple DML
INSERT INTO dml_tab_smallint values(-32768);
SELECT * FROM dml_tab_smallint ORDER BY 1;
INSERT INTO dml_tab_smallint values(32767);
SELECT * FROM dml_tab_smallint ORDER BY 1;
UPDATE dml_tab_smallint SET a = 32767;
SELECT * FROM dml_tab_smallint ORDER BY 1;
UPDATE dml_tab_smallint SET a = -32768;
SELECT * FROM dml_tab_smallint ORDER BY 1;

-- OUT OF RANGE
INSERT INTO dml_tab_smallint values(-32769);
SELECT * FROM dml_tab_smallint ORDER BY 1;
INSERT INTO dml_tab_smallint values(32768);
SELECT * FROM dml_tab_smallint ORDER BY 1;
UPDATE dml_tab_smallint SET a = 32768;
SELECT * FROM dml_tab_smallint ORDER BY 1;
UPDATE dml_tab_smallint SET a = -32769;
SELECT * FROM dml_tab_smallint ORDER BY 1;





-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test1: Boundary test for text



DROP TABLE IF EXISTS dml_text;
CREATE TABLE dml_text( a text) distributed by (a);

-- Simple DML
INSERT INTO dml_text VALUES(array_to_string(ARRAY(SELECT chr((65 + round(random() * 25)) :: integer) FROM generate_series(1, 10490000)), ''));

UPDATE dml_text SET a = array_to_string(ARRAY(SELECT chr((65 + round(random() * 25)) :: integer) FROM generate_series(1, 10490000)), '');


-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for timestamp




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

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for timestamptz




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
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for varchar




DROP TABLE IF EXISTS dml_var;
CREATE TABLE dml_var( a varchar(10485760)) distributed by (a);

-- Simple DML
INSERT INTO dml_var VALUES(repeat('x',10485760));
UPDATE dml_var SET a = 'y';
SELECT * FROM dml_var ORDER BY 1;
-- OUT OF RANGE
INSERT INTO dml_var VALUES(repeat('x',10485761));
UPDATE dml_var SET a = repeat('x',10485761);
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for box




DROP TABLE IF EXISTS dml_box;
CREATE TABLE dml_box(a box) DISTRIBUTED RANDOMLY;
-- Simple DML
INSERT INTO dml_box VALUES ('(8,9), (1,3)');
SELECT area((select * from dml_box));

UPDATE dml_box SET a = '(1,1), (1,1)';
SELECT area((select * from dml_box));

-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test1: Boundary test for circle




DROP TABLE IF EXISTS dml_circle;
CREATE TABLE dml_circle(a circle) DISTRIBUTED RANDOMLY;

-- Simple DML
INSERT INTO dml_circle VALUES ('10, 4, 10');
SELECT area((select * from dml_circle));

UPDATE dml_circle SET a = '1,1,1';
SELECT area((select * from dml_circle));
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Insert on CO tables within transaction




DROP TABLE IF EXISTS dml_co;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_co;
    CREATE TABLE dml_co (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true, orientation=column) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
SAVEPOINT sp2;
    INSERT INTO dml_co values (1,'to be exchanged','a','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
ROLLBACK to sp2;
    INSERT INTO dml_co values (1,'exchanged','a','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_co;
SELECT x FROM dml_co;
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Negative test - Update on CO tables within transaction




DROP TABLE IF EXISTS dml_co;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_co;
    CREATE TABLE dml_co (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true, orientation=column) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_co values (7,'to be exchanged','s','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    UPDATE dml_co SET x = 'update_co';
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_co;
SELECT x FROM dml_co;
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Negative test - Delete on CO tables within transaction




DROP TABLE IF EXISTS dml_co;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_co;
    CREATE TABLE dml_co (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) with(appendonly=true, orientation=column) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_co values(generate_series(1,9),'create table with subtransactions','s','subtransaction table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    DELETE FROM dml_co;
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_co;
SELECT x FROM dml_co;



-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Insert on heap tables within transaction




DROP TABLE IF EXISTS dml_heap;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_heap;
    CREATE TABLE dml_heap (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
SAVEPOINT sp2;
    INSERT INTO dml_heap values (1,'to be exchanged','a','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
ROLLBACK to sp2;
    INSERT INTO dml_heap values (1,'to be exchanged','a','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_heap;
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Insert on heap tables within transaction 




DROP TABLE IF EXISTS dml_heap;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_heap;
    CREATE TABLE dml_heap (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_heap values (7,'to be exchanged','s','partition table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    UPDATE dml_heap SET x = 'update';
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_heap;
-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Insert on heap tables within transaction 




DROP TABLE IF EXISTS dml_heap;
BEGIN;
SAVEPOINT sp1;
    DROP TABLE IF EXISTS dml_heap;
    CREATE TABLE dml_heap (i int, x text,c char,v varchar, d date, n numeric, t timestamp without time zone, tz time with time zone) distributed by (i) partition by range (i) (partition p1 start(1) end(5),partition p2 start(5) end(8), partition p3 start(8) end(10) );
    INSERT INTO dml_heap values(generate_series(1,9),'create table with subtransactions','s','subtransaction table','12-11-2012',3,'2012-10-09 10:23:54', '2011-08-19 10:23:54+02');
SAVEPOINT sp2;
    DELETE FROM dml_heap;
RELEASE SAVEPOINT sp2;
RELEASE SAVEPOINT sp1;
COMMIT;

SELECT COUNT(*) FROM dml_heap;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for path




DROP TABLE IF EXISTS dml_path;
CREATE TABLE dml_path(a path) DISTRIBUTED RANDOMLY;

-- Simple DML
INSERT INTO dml_path VALUES ('(3,1), (2,8), (10,4)');
SELECT length((select * from dml_path));

UPDATE dml_path SET a = '(4,2), (3,2), (11,5)';
SELECT length((select * from dml_path));
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for point




DROP TABLE IF EXISTS dml_point;
CREATE TABLE dml_point(a point) DISTRIBUTED RANDOMLY;

-- Simple DML
INSERT INTO dml_point VALUES ('1,2');
SELECT * FROM dml_point;

UPDATE dml_point SET a = '2,3';
SELECT * FROM dml_point;


-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test: Boundary test for polygon




DROP TABLE IF EXISTS dml_polygon;
CREATE TABLE dml_polygon (a polygon) DISTRIBUTED RANDOMLY;

-- Simple DML 
INSERT INTO dml_polygon VALUES ('((2,2),(3,4),(3,6),(1,1))');
SELECT point(a) FROM dml_polygon;

UPDATE dml_polygon SET a = '((2,2),(3,3),(4,4),(1,1))';
SELECT point(a) FROM dml_polygon;



-- @author prabhd
-- @created 2012-12-05 12:00:00
-- @modified 2012-12-05 12:00:00
-- @tags dml
-- @db_name dmldb
-- @description test: Boundary test for serial



DROP SEQUENCE IF EXISTS serial;
CREATE SEQUENCE serial START 101;

DROP TABLE IF EXISTS dml_serial;
CREATE TABLE dml_serial(a serial) distributed by (a);

INSERT INTO dml_serial VALUES (nextval('serial'));
INSERT INTO dml_serial VALUES (nextval('serial'));

SELECT * FROM dml_serial ORDER BY 1;
UPDATE dml_serial set a = nextval('serial');

SELECT * FROM dml_serial ORDER BY 1;

UPDATE dml_serial set a = nextval('serial');

SELECT * FROM dml_serial ORDER BY 1;
-- @author xiongg1 
-- @created 2015-01-20 12:00:00
-- @modified 2015-01-20 12:00:00
-- @tags dml ORCA
-- @product_version gpdb: [4.3.5.0-]
-- @description Tests for MPP-24621

select to_date('-4713-11-23', 'yyyy-mm-dd');
select to_date('-4713-11-24', 'yyyy-mm-dd');
select to_date('5874897-12-31', 'yyyy-mm-dd');
select to_date('5874898-01-01', 'yyyy-mm-dd');

-- start_ignore
drop table if exists t_to_date;
create table t_to_date(c1 date);
-- end_ignore
insert into t_to_date values (to_date('-4713-11-24', 'yyyy-mm-dd'));
insert into t_to_date values (to_date('5874897-12-31', 'yyyy-mm-dd'));
select * from t_to_date;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test1: UDF with Insert




-- start_ignore
drop schema dml_functional_no_setup_1 cascade;
-- end_ignore
